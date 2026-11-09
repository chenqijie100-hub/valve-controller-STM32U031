#include "valve_comm_service.h"
#include "comm_port.h"

static CommStatus status;
static uint32_t started, stopped, attempted, in_flight;
static uint16_t previous_position, previous_faults;
static uint8_t observed, has_stopped, tried, retries, deferred, battery_blocked;
static uint8_t request[MB_REQUEST_SIZE];

static uint16_t position(const AppStatus *s)
{
  switch (s->valve.state)
  {
    case VALVE_OPENED: return MB_POS_OPEN;
    case VALVE_CLOSED: return MB_POS_CLOSED;
    case VALVE_OPENING: return MB_POS_OPENING;
    case VALVE_CLOSING: return MB_POS_CLOSING;
    case VALVE_FAULT: return MB_POS_FAULT;
    default:
      return s->inputs.limits_invalid || s->inputs.open_limit || s->inputs.close_limit
          ? MB_POS_UNKNOWN : MB_POS_MIDDLE;
  }
}

static uint16_t faults(const AppStatus *s)
{
  uint32_t valve = s->valve.current_faults | s->valve.latched_faults;
  uint16_t result = 0;
  if ((valve & VALVE_FAULT_WIRE) || s->inputs.wire_break) result |= MB_FAULT_WIRE;
  if (valve & VALVE_FAULT_DRIVER) result |= MB_FAULT_DRIVER;
  if (valve & VALVE_FAULT_TIMEOUT) result |= MB_FAULT_TIMEOUT;
  if (valve & VALVE_FAULT_BOTH_LIMITS) result |= MB_FAULT_LIMITS;
  if (status.battery_low) result |= MB_FAULT_BATTERY;
  if (status.power_fault) result |= MB_FAULT_9V;
  return result;
}

static void queue(uint16_t events)
{
  if (!events) return;
  status.pending_events |= events;
  ++status.sequence;
}

static void observe(const AppStatus *s)
{
  if (s->battery.valid && s->battery.millivolts < MB_MIN_BATTERY_MV)
    status.battery_low = 1;
  uint16_t pos = position(s), fault = faults(s), events = 0;
  if (observed && (pos != previous_position ||
      ((fault ^ previous_faults) & ~(MB_FAULT_BATTERY | MB_FAULT_9V))))
    battery_blocked = 0;
  if (!observed) events |= MB_EVENT_START;
  else
  {
    if (pos != previous_position) events |= MB_EVENT_POSITION;
    if ((fault ^ previous_faults) & MB_FAULT_WIRE) events |= MB_EVENT_WIRE;
    if ((fault ^ previous_faults) & ~MB_FAULT_WIRE) events |= MB_EVENT_FAULT;
  }
  previous_position = pos;
  previous_faults = fault;
  observed = 1;
  queue(events);
}

void ValveComm_Init(void)
{
  status = (CommStatus){0};
  observed = has_stopped = tried = deferred = battery_blocked = 0;
  if (VALVE_COMM_9V_ENABLE) CommPower_Set(0);
}

static void finish(uint32_t now, ModbusResult result)
{
  CommPort_Disable();
  if (VALVE_COMM_9V_ENABLE) CommPower_Set(0);
  stopped = now;
  has_stopped = 1;
  status.state = COMM_IDLE;
  deferred = status.pending_events && (!tried || attempted != status.sequence);
  status.result = result;
}

static void retry(uint32_t now, ModbusResult result)
{
  status.result = result;
  if (retries == MB_RETRIES || result == MB_EXCEPTION)
    finish(now, result);
  else
  {
    ++retries;
    started = now;
    status.state = COMM_RETRY;
  }
}

void ValveComm_Service(uint32_t now, const AppStatus *s, uint8_t safety_busy)
{
  observe(s);
  safety_busy |= s->valve.safety_close_pending || s->nsleep || s->pwm_percent != 0;
  uint32_t elapsed = now - started;
  uint8_t battery_ok = s->battery.valid && s->battery.millivolts >= MB_MIN_BATTERY_MV &&
                       s->battery.millivolts <= UINT16_MAX;

  if (VALVE_COMM_9V_ENABLE && status.state != COMM_IDLE &&
      status.state != COMM_PREPARE && CommPower_Fault())
    goto protection_fault;
  if (status.state != COMM_IDLE && safety_busy)
  {
    tried = 0; /* A safety interruption is not a failed reporting attempt. */
    finish(now, MB_PREEMPTED);
    return;
  }

  if (status.state != COMM_IDLE && !battery_ok)
  {
    /* An unloaded voltage recovery alone must not cause repeated power cycling. */
    battery_blocked = 1;
    attempted = status.sequence;
    finish(now, MB_BATTERY_UNAVAILABLE);
    return;
  }

  switch (status.state)
  {
    case COMM_IDLE:
      deferred = status.pending_events && !safety_busy && battery_ok && !battery_blocked &&
                 (!tried || attempted != status.sequence);
      if (!deferred) break;
      if (VALVE_COMM_9V_ENABLE && has_stopped && now - stopped < MB_POWER_OFF_MIN_MS) break;
      deferred = 0;
      if (VALVE_COMM_9V_ENABLE) CommPower_Set(1);
      CommPort_Enable();
      attempted = status.sequence;
      tried = 1;
      retries = 0;
      status.exception = 0;
      started = now;
      status.state = COMM_PREPARE;
      break;

    case COMM_PREPARE:
      if (VALVE_COMM_9V_ENABLE)
      {
        if (elapsed < MB_POWER_SETTLE_MS) break;
        if (CommPower_Fault()) goto protection_fault;
      }
      in_flight = status.sequence;
      attempted = in_flight;
      {
        uint16_t regs[MB_REGISTER_COUNT] = {
          MB_VERSION, position(s), (uint16_t)s->battery.millivolts, faults(s),
          status.pending_events, (uint16_t)(in_flight >> 16), (uint16_t)in_flight
        };
        Modbus_Encode(regs, request);
      }
      started = now;
      status.state = COMM_TX_REQUEST;
      break;

    case COMM_RETRY:
    case COMM_TX_REQUEST:
      if (elapsed >= MB_SEND_DEADLINE_MS) finish(now, MB_TRANSPORT);
      else if (CommPort_TrySend(request))
      {
        started = now;
        status.state = COMM_WAIT_TX;
      }
      break;

    case COMM_WAIT_TX:
      if (CommPort_TxDone())
      {
        started = now;
        status.state = COMM_WAIT_RESPONSE;
      }
      else if (elapsed >= MB_SEND_DEADLINE_MS) finish(now, MB_TRANSPORT);
      break;

    case COMM_WAIT_RESPONSE:
    {
      uint8_t reply[MB_RESPONSE_SIZE];
      int length = CommPort_Receive(reply);
      if (elapsed >= MB_RESPONSE_MS) retry(now, MB_TIMEOUT);
      else if (length < 0) retry(now, MB_TRANSPORT);
      else if (length > 0)
      {
        ModbusResult result = Modbus_Check(reply, (unsigned)length, &status.exception);
        if (result != MB_OK) retry(now, result);
        else
        {
          status.acknowledged = in_flight;
          if (status.sequence == in_flight) status.pending_events = 0;
          if (status.sequence == in_flight && (status.power_fault || status.battery_low))
          {
            status.power_fault = status.battery_low = 0;
            observe(s); /* Queue a separate recovery report. */
          }
          finish(now, MB_OK);
        }
      }
      break;
    }
  }
  return;

protection_fault:
  status.power_fault = 1;
  observe(s);
  attempted = status.sequence; /* Do not power-cycle because of our own fault event. */
  finish(now, MB_POWER_FAULT);
}

void ValveComm_RequestPeriodic(void)
{
  battery_blocked = 0;
  queue(MB_EVENT_PERIODIC);
}
const CommStatus *ValveComm_Get(void) { return &status; }
uint8_t ValveComm_Busy(void) { return status.state != COMM_IDLE || deferred; }
