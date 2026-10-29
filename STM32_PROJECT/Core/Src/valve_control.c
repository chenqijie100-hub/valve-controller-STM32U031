#include "valve_control.h"
#include "valve_port.h"

_Static_assert(VALVE_REVERSAL_WAIT_MS >= VALVE_SLEEP_SETTLE_MS,
               "Reversal wait must include the validated sleep settling time");

/* Main context owns state. EXTI communicates only through the port. */
static ValveStatus status;
static ValveDriveStage stage;
static ValveDirection direction, reached, manual_direction;
static uint32_t stop_tick, stage_tick, run_tick, jog_duration, run_duty;
static uint32_t din_tick, wire_low_tick, rearm_low_tick;
static uint8_t din_candidate, din_stable, din_valid;
static uint8_t wire_active, wire_low_timing, rearm_required, rearm_low_timing;
static uint8_t manual_active, driver_seen;
static uint8_t sleep_wire_level,sleep_request_level;
static uint8_t limits_were_invalid;

static void stop_drive(uint32_t now)
{
  (void)ValvePort_Apply(VALVE_DRIVE_OFF, VALVE_DIR_NONE, 0);
  stage = VALVE_DRIVE_OFF;
  stop_tick = now;
}

static void latch(uint32_t faults)
{
  status.latched_faults |= faults;
  status.history_faults |= faults;
}

static void assert_break(void)
{
  wire_active = 1;
  wire_low_timing = 0;
  rearm_required = 1;
  rearm_low_timing = 0;
  status.opening_permitted = 0;
  status.safety_close_pending = 1;
  status.history_faults |= VALVE_FAULT_WIRE;
  manual_active = 0;
  reached = VALVE_DIR_NONE;
}

static void refresh_faults(const ValveInputs *in)
{
  status.current_faults = status.latched_faults &
      (VALVE_FAULT_START | VALVE_FAULT_SAFETY_INCOMPLETE);
  if (wire_active) status.current_faults |= VALVE_FAULT_WIRE;
  if (!in->limits_invalid && in->open_limit && in->close_limit) status.current_faults |= VALVE_FAULT_BOTH_LIMITS;
  if (driver_seen && in->driver_fault) status.current_faults |= VALVE_FAULT_DRIVER;
  status.history_faults |= status.current_faults;
}

static uint8_t apply(ValveDriveStage next, uint32_t now)
{
  ValvePortResult result = ValvePort_Apply(next, direction, run_duty);
  if (result == VALVE_PORT_OK)
  {
    stage = next; stage_tick = now;
    if (next == VALVE_DRIVE_RUN) run_tick = now;
    return 1;
  }
  stop_drive(now);
  if (result == VALVE_PORT_BREAK) assert_break();
  else if (result == VALVE_PORT_DRIVER) { driver_seen = 1; latch(VALVE_FAULT_DRIVER); }
  else if (result == VALVE_PORT_ERROR) latch(VALVE_FAULT_START);
  /* LIMIT is re-evaluated from fresh inputs next step. No drive was enabled. */
  if (status.safety_close_pending && status.latched_faults)
    latch(VALVE_FAULT_SAFETY_INCOMPLETE);
  status.current_faults |= status.latched_faults;
  if (wire_active) status.current_faults |= VALVE_FAULT_WIRE;
  status.state = status.latched_faults ? VALVE_FAULT : VALVE_UNKNOWN;
  return 0;
}

void Valve_Init(uint32_t now)
{
  status = (ValveStatus){.state = VALVE_UNKNOWN, .opening_permitted = 1, .digital_enabled = 1};
  direction = reached = manual_direction = VALVE_DIR_NONE;
  din_candidate = din_stable = din_valid = 0;
  wire_active = wire_low_timing = rearm_required = rearm_low_timing = 0;
  manual_active = driver_seen = 0;
  din_tick = wire_low_tick = rearm_low_tick = now;
  jog_duration = 0; run_duty = 100;
  sleep_wire_level=sleep_request_level=0;
  limits_were_invalid=0;
  stop_drive(now);
}

const ValveStatus *Valve_Get(void) { return &status; }
uint8_t Valve_CanSleep(uint32_t now)
{
  /* Observation only: never changes motion, deadlines or recovery interlocks. */
  if(stage!=VALVE_DRIVE_OFF || status.safety_close_pending || manual_active)return 0;
  if(!din_valid || din_candidate!=din_stable ||
     (uint32_t)(now-din_tick)<VALVE_INPUT_STABLE_MS)return 0;
  if(wire_active && !sleep_wire_level)return 0;
  if(rearm_required && !wire_active && !sleep_request_level)return 0;
  if(status.latched_faults)return 1;
  if(status.digital_enabled)
  {
    ValveDirection desired=din_stable?VALVE_DIR_OPEN:VALVE_DIR_CLOSE;
    if(desired==VALVE_DIR_OPEN && !status.opening_permitted)return 1;
    if(reached!=desired)return 0; /* Includes deferred reversal/cooldown. */
  }
  return 1;
}

void Valve_Step(uint32_t now, const ValveInputs *in)
{
  sleep_wire_level=in->wire_break; sleep_request_level=in->request_open;
  ValveDirection desired = VALVE_DIR_NONE;
  /* Assertion is immediate; release is qualified. Never discard a pulse. */
  if (in->break_event || (in->wire_break && !wire_active)) assert_break();
  if (in->wire_break) wire_low_timing = 0;
  else if (wire_active)
  {
    if (!wire_low_timing) { wire_low_timing = 1; wire_low_tick = now; }
    if ((uint32_t)(now - wire_low_tick) >= VALVE_WIRE_RECOVERY_MS)
    { wire_active = 0; rearm_low_timing = 0; }
  }
  if (rearm_required && !wire_active)
  {
    if (in->request_open) rearm_low_timing = 0;
    else
    {
      if (!rearm_low_timing) { rearm_low_timing = 1; rearm_low_tick = now; }
      if ((uint32_t)(now - rearm_low_tick) >= VALVE_INPUT_STABLE_MS)
      { rearm_required = 0; status.opening_permitted = 1; }
    }
  }
  if (in->request_open != din_candidate)
  { din_candidate = in->request_open; din_tick = now; }
  if ((uint32_t)(now - din_tick) >= VALVE_INPUT_STABLE_MS &&
      (!din_valid || din_stable != din_candidate))
  { din_valid = 1; din_stable = din_candidate; reached = VALVE_DIR_NONE; }

  /* A powered-down domain is a cached position, not a reached latch for the
   * next observation. Revalidate the destination from fresh stable inputs. */
  if(limits_were_invalid && !in->limits_invalid)
  { reached=VALVE_DIR_NONE;if(stage==VALVE_DRIVE_OFF)status.state=VALVE_UNKNOWN; }
  limits_were_invalid=in->limits_invalid;

  /* Collect simultaneous protection faults BEFORE destination handling. */
  if (!in->limits_invalid && in->open_limit && in->close_limit) latch(VALVE_FAULT_BOTH_LIMITS);
  if (in->driver_fault && (stage == VALVE_DRIVE_RUN ||
      (stage == VALVE_DRIVE_WAKE && (uint32_t)(now - stage_tick) >= VALVE_WAKE_SETTLE_MS)))
  { driver_seen = 1; latch(VALVE_FAULT_DRIVER); }
  if (status.latched_faults)
  {
    if (status.safety_close_pending && (in->limits_invalid || !in->close_limit || in->open_limit))
      latch(VALVE_FAULT_SAFETY_INCOMPLETE);
    if (stage != VALVE_DRIVE_OFF) stop_drive(now);
    manual_active = 0; status.opening_permitted = 0; status.state = VALVE_FAULT;
    refresh_faults(in); return;
  }

  if (in->limits_invalid)
  {
    if (stage != VALVE_DRIVE_OFF)
    { stop_drive(now); reached = VALVE_DIR_NONE; status.state = VALVE_UNKNOWN; }
    if (status.safety_close_pending ||
        (status.state != VALVE_OPENED && status.state != VALVE_CLOSED))status.state = VALVE_UNKNOWN;
    refresh_faults(in); return; /* L4/rearm/DIN above remain live; no position decision. */
  }

  if (status.safety_close_pending)
  {
    if (in->close_limit && !in->open_limit)
    {
      if (stage != VALVE_DRIVE_OFF) stop_drive(now);
      status.safety_close_pending = 0; reached = VALVE_DIR_CLOSE;
      status.state = VALVE_CLOSED; refresh_faults(in); return;
    }
    desired = VALVE_DIR_CLOSE;
  }
  else if (manual_active) desired = manual_direction;
  else if (status.digital_enabled && din_valid)
    desired = din_stable ? VALVE_DIR_OPEN : VALVE_DIR_CLOSE;
  if (desired == VALVE_DIR_OPEN && !status.opening_permitted) desired = VALVE_DIR_NONE;
  if (stage != VALVE_DRIVE_OFF && direction != desired)
  { stop_drive(now); status.state = VALVE_UNKNOWN; }

  if (stage != VALVE_DRIVE_OFF)
  {
    if ((direction == VALVE_DIR_OPEN && in->open_limit) ||
        (direction == VALVE_DIR_CLOSE && in->close_limit))
    {
      reached = direction; stop_drive(now); manual_active = 0;
      status.state = direction == VALVE_DIR_OPEN ? VALVE_OPENED : VALVE_CLOSED;
    }
    else if (stage == VALVE_DRIVE_RUN)
    {
      uint32_t elapsed = now - run_tick;
      if (jog_duration && elapsed >= jog_duration)
      { stop_drive(now); manual_active = 0; status.state = VALVE_UNKNOWN; }
    }
    else if (stage == VALVE_DRIVE_REFERENCE && (uint32_t)(now - stage_tick) >= VALVE_VREF_SETTLE_MS)
      (void)apply(VALVE_DRIVE_WAKE, now);
    else if (stage == VALVE_DRIVE_WAKE && (uint32_t)(now - stage_tick) >= VALVE_WAKE_SETTLE_MS)
      (void)apply(VALVE_DRIVE_RUN, now);
  }
  else if (desired != VALVE_DIR_NONE && reached != desired)
  {
    if ((desired == VALVE_DIR_OPEN && in->open_limit) ||
        (desired == VALVE_DIR_CLOSE && in->close_limit))
    {
      reached = desired; manual_active = 0;
      status.state = desired == VALVE_DIR_OPEN ? VALVE_OPENED : VALVE_CLOSED;
    }
    else if ((uint32_t)(now - stop_tick) >= VALVE_REVERSAL_WAIT_MS)
    {
      direction = desired;
      if (status.safety_close_pending || !manual_active) { run_duty = 100; jog_duration = 0; }
      if (apply(VALVE_DRIVE_REFERENCE, now))
        status.state = direction == VALVE_DIR_OPEN ? VALVE_OPENING : VALVE_CLOSING;
    }
  }
  else if (stage == VALVE_DRIVE_OFF && reached == VALVE_DIR_NONE)
    status.state = in->open_limit ? VALVE_OPENED : in->close_limit ? VALVE_CLOSED : VALVE_UNKNOWN;
  refresh_faults(in);
}

uint8_t Valve_Request(ValveDirection dir, uint32_t jog_ms, uint32_t duty)
{
  if ((dir != VALVE_DIR_OPEN && dir != VALVE_DIR_CLOSE) || !duty || duty > 100 ||
      (jog_ms != 0 && jog_ms != 100) || stage != VALVE_DRIVE_OFF ||
      status.latched_faults || status.safety_close_pending ||
      wire_active || (dir == VALVE_DIR_OPEN && !status.opening_permitted)) return 0;
  status.digital_enabled = 0;
  manual_direction = dir; jog_duration = jog_ms; run_duty = duty;
  manual_active = 1; reached = VALVE_DIR_NONE;
  return 1;
}

uint8_t Valve_Stop(uint32_t now)
{
  if (status.safety_close_pending || wire_active) return 0;
  status.digital_enabled = 0; manual_active = 0; reached = VALVE_DIR_NONE;
  stop_drive(now);
  if (!status.latched_faults) status.state = VALVE_UNKNOWN;
  return 1;
}

uint8_t Valve_SetDigital(uint8_t enabled, uint32_t now)
{
  if (!enabled) return Valve_Stop(now);
  if (status.latched_faults) return 0;
  status.digital_enabled = 1; manual_active = 0;
  return 1;
}

uint8_t Valve_ClearFaults(uint32_t now, const ValveInputs *in)
{
  if (in->limits_invalid || !status.latched_faults || stage != VALVE_DRIVE_OFF || wire_active || in->wire_break || in->break_event ||
      in->driver_fault || (in->open_limit && in->close_limit)) return 0;
  status.latched_faults = 0; driver_seen = 0; status.state = VALVE_UNKNOWN;
  status.opening_permitted = 0; rearm_required = 1; rearm_low_timing = 0;
  manual_active = 0; reached = VALVE_DIR_NONE; stop_tick = now;
  refresh_faults(in);
  return 1;
}

const char *Valve_StateName(ValveState s)
{
  switch (s) {
    case VALVE_OPENING: return "OPENING";
    case VALVE_OPENED: return "OPENED";
    case VALVE_CLOSING: return "CLOSING";
    case VALVE_CLOSED: return "CLOSED";
    case VALVE_FAULT: return "FAULT";
    default: return "UNKNOWN";
  }
}
