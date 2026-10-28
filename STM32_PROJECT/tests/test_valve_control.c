#include "valve_control.h"
#include "valve_port.h"

#define CHECK(x) do { if (!(x)) return __LINE__; } while (0)
unsigned tests_passed;
static ValveInputs input;
static uint32_t now;
static ValveDriveStage output;
static ValveDirection direction;
static uint32_t duty;
static ValvePortResult injected;

void *memset(void *p, int value, unsigned size)
{ unsigned char *b = p; while (size--) *b++ = (unsigned char)value; return p; }
ValvePortResult ValvePort_Apply(ValveDriveStage stage, ValveDirection dir, uint32_t percent)
{
  if (stage != VALVE_DRIVE_OFF && injected != VALVE_PORT_OK) return injected;
  output = stage; direction = dir; duty = percent; return VALVE_PORT_OK;
}
static void step(uint32_t elapsed) { now += elapsed; Valve_Step(now, &input); }
static void reset(void)
{
  input = (ValveInputs){0}; now = 0; injected = VALVE_PORT_OK;
  Valve_Init(now); step(0);
}
static void start(void) { step(50); step(1950); step(20); step(2); }

int run_tests(void)
{
  /* Default digital control, fresh startup settling/cooldown, proven direction. */
  reset(); CHECK(output == VALVE_DRIVE_OFF);
  input.request_open = 1; step(1); step(49); CHECK(output == VALVE_DRIVE_OFF);
  step(1950); step(20); step(2);
  CHECK(Valve_Get()->state == VALVE_OPENING && output == VALVE_DRIVE_RUN);
  CHECK(direction == VALVE_DIR_OPEN && duty == 100); tests_passed++;

  /* Destination stop; no restart on release, but can leave opposite limit. */
  input.open_limit = 1; step(1); CHECK(Valve_Get()->state == VALVE_OPENED);
  input.open_limit = 0; step(3000); CHECK(output == VALVE_DRIVE_OFF);
  input.open_limit = 1; input.request_open = 0; step(0); step(50);
  step(20); step(2); CHECK(direction == VALVE_DIR_CLOSE && output == VALVE_DRIVE_RUN);
  input.open_limit = 0; input.close_limit = 1; step(1);
  CHECK(Valve_Get()->state == VALVE_CLOSED); tests_passed++;

  /* Break interrupts opening; closing survives pin recovery and high command. */
  reset(); input.request_open = 1; start();
  input.wire_break = 1; step(1); CHECK(output == VALVE_DRIVE_OFF);
  CHECK(Valve_Get()->safety_close_pending && !Valve_Get()->opening_permitted);
  input.wire_break = 0; step(1); step(100); CHECK(!(Valve_Get()->current_faults & VALVE_FAULT_WIRE));
  step(1899); step(20); step(2);
  CHECK(output == VALVE_DRIVE_RUN && direction == VALVE_DIR_CLOSE);
  CHECK(!Valve_Request(VALVE_DIR_OPEN, 0, 100)); CHECK(!Valve_Stop(now));
  CHECK(!Valve_SetDigital(0, now));
  input.close_limit = 1; step(1); CHECK(Valve_Get()->state == VALVE_CLOSED);
  step(3000); CHECK(output == VALVE_DRIVE_OFF);
  CHECK(!Valve_Request(VALVE_DIR_OPEN, 100, 50));
  input.request_open = 0; step(1); step(49); CHECK(!Valve_Get()->opening_permitted);
  step(1); CHECK(Valve_Get()->opening_permitted);
  input.request_open = 1; step(1); step(50); step(20); step(2);
  CHECK(output == VALVE_DRIVE_RUN && direction == VALVE_DIR_OPEN); tests_passed++;

  /* A pulse recorded by EXTI still forces closure after raw pin is low. */
  reset(); input.request_open = 1; start(); input.break_event = 1; step(1);
  input.break_event = 0; CHECK(Valve_Get()->safety_close_pending);
  step(2000); step(20); step(2); CHECK(direction == VALVE_DIR_CLOSE); tests_passed++;

  /* Every startup phase accepts a break without an opening duty write. */
  for (unsigned phase = 0; phase < 3; ++phase)
  {
    reset(); input.request_open = 1; step(50); step(1950);
    if (phase >= 1) step(20);
    if (phase >= 2) step(1);
    input.wire_break = 1; step(1);
    CHECK(output == VALVE_DRIVE_OFF && Valve_Get()->safety_close_pending);
    step(2000); step(20); step(2);
    CHECK(output == VALVE_DRIVE_RUN && direction == VALVE_DIR_CLOSE); tests_passed++;
  }

  /* Continuous high does not perpetually restart the close sequence. */
  reset(); input.wire_break = 1; start();
  CHECK(output == VALVE_DRIVE_RUN && direction == VALVE_DIR_CLOSE);
  step(10); CHECK(output == VALVE_DRIVE_RUN);
  input.close_limit = 1; step(1); CHECK(Valve_Get()->state == VALVE_CLOSED);
  step(3000); CHECK(output == VALVE_DRIVE_OFF); tests_passed++;

  /* A real driver fault latches incomplete; recovery needs explicit clear. */
  reset(); input.wire_break = 1; start(); input.driver_fault = 1; step(1);
  CHECK(output == VALVE_DRIVE_OFF && Valve_Get()->state == VALVE_FAULT);
  CHECK(Valve_Get()->latched_faults & VALVE_FAULT_DRIVER);
  CHECK(Valve_Get()->current_faults & VALVE_FAULT_SAFETY_INCOMPLETE);
  CHECK(!Valve_ClearFaults(now, &input)); step(5000); CHECK(output == VALVE_DRIVE_OFF);
  input.driver_fault = 0; input.wire_break = 0; step(1); step(100);
  CHECK(Valve_ClearFaults(now, &input));
  CHECK(Valve_Get()->history_faults & VALVE_FAULT_DRIVER);
  step(2000); step(20); step(2); CHECK(output == VALVE_DRIVE_RUN && direction == VALVE_DIR_CLOSE);
  tests_passed++;

  /* Both limits and nFAULT are recorded even alongside arrival; no false CLOSED. */
  reset(); input.wire_break = 1; start(); input.open_limit = input.close_limit = 1;
  input.driver_fault = 1; step(1);
  CHECK(Valve_Get()->state == VALVE_FAULT && output == VALVE_DRIVE_OFF);
  CHECK((Valve_Get()->latched_faults & (VALVE_FAULT_DRIVER | VALVE_FAULT_BOTH_LIMITS)) ==
        (VALVE_FAULT_DRIVER | VALVE_FAULT_BOTH_LIMITS));
  CHECK(Valve_Get()->history_faults & VALVE_FAULT_SAFETY_INCOMPLETE);
  CHECK(!Valve_Request(VALVE_DIR_CLOSE, 0, 100)); tests_passed++;

  /* Normal opening has no 30-second or larger software runtime deadline. */
  reset(); input.request_open = 1; start(); step(30000); step(60000); step(3600000);
  CHECK(output == VALVE_DRIVE_RUN && Valve_Get()->state == VALVE_OPENING);
  CHECK(!Valve_Get()->latched_faults);
  input.open_limit = 1; step(1); CHECK(output == VALVE_DRIVE_OFF && Valve_Get()->state == VALVE_OPENED);
  tests_passed++;

  /* Normal closing also runs until its destination limit, however long it takes. */
  reset(); start(); step(30000); step(3600000);
  CHECK(output == VALVE_DRIVE_RUN && Valve_Get()->state == VALVE_CLOSING);
  input.close_limit = 1; step(1); CHECK(output == VALVE_DRIVE_OFF && Valve_Get()->state == VALVE_CLOSED);
  tests_passed++;

  /* Safety closing survives the former timeout and wire recovery. */
  reset(); input.wire_break = 1; start(); step(60000);
  input.wire_break = 0; step(1); step(100); step(3600000);
  CHECK(output == VALVE_DRIVE_RUN && Valve_Get()->safety_close_pending);
  CHECK(!(Valve_Get()->history_faults & VALVE_FAULT_TIMEOUT));
  input.close_limit = 1; step(1); CHECK(output == VALVE_DRIVE_OFF && Valve_Get()->state == VALVE_CLOSED);
  tests_passed++;

  /* Jog is 50%, bounded, and uses the same safeguards. */
  reset(); CHECK(Valve_Request(VALVE_DIR_OPEN, 100, 50)); step(2000); step(20); step(2);
  CHECK(duty == 50 && output == VALVE_DRIVE_RUN); step(100);
  CHECK(output == VALVE_DRIVE_OFF && !(Valve_Get()->latched_faults & VALVE_FAULT_TIMEOUT));
  CHECK(!Valve_Request(VALVE_DIR_OPEN, 0, 101)); tests_passed++;

  /* Port guard sees a break after Step sampled inputs: no start-failure retry. */
  reset(); input.request_open = 1; step(50); step(1950); step(20);
  injected = VALVE_PORT_BREAK; step(2); CHECK(output != VALVE_DRIVE_RUN);
  CHECK(Valve_Get()->safety_close_pending && !Valve_Get()->opening_permitted);
  tests_passed++;

  /* Stable low must be observed after recovery, not borrowed from before it. */
  reset(); input.wire_break = 1; input.close_limit = 1; step(50);
  input.wire_break = 0; step(1); step(99); CHECK(!Valve_Get()->opening_permitted);
  step(1); CHECK(!Valve_Get()->opening_permitted); step(49); CHECK(!Valve_Get()->opening_permitted);
  step(1); CHECK(Valve_Get()->opening_permitted); tests_passed++;

  /* Unsigned timer arithmetic across rollover. */
  input = (ValveInputs){.request_open = 1}; now = 0xfffffff0U;
  injected = VALVE_PORT_OK; Valve_Init(now); step(0); start();
  CHECK(output == VALVE_DRIVE_RUN); tests_passed++;

  /* Previously reached-close memory cannot suppress a later safety close. */
  reset(); input.close_limit = 1; step(50); CHECK(Valve_Get()->state == VALVE_CLOSED);
  input.close_limit = 0; step(3000); CHECK(output == VALVE_DRIVE_OFF);
  input.wire_break = 1; step(1); step(20); step(2);
  CHECK(output == VALVE_DRIVE_RUN && direction == VALVE_DIR_CLOSE); tests_passed++;

  /* Repeated RUN/JOG cannot alter active duty or extend active timing. */
  reset(); input.request_open = 1; start();
  CHECK(!Valve_Request(VALVE_DIR_OPEN, 100, 50)); tests_passed++;

  /* Arrival after the former deadline is a normal successful arrival. */
  reset(); input.request_open = 1; start(); input.open_limit = 1;
  step(60000);
  CHECK(Valve_Get()->state == VALVE_OPENED && !Valve_Get()->latched_faults);
  tests_passed++;
  /* A fault-clear command without a protective fault cannot postpone closure. */
  reset(); input.wire_break = 1; step(1); input.wire_break = 0;
  step(1); step(100); CHECK(Valve_Get()->safety_close_pending);
  CHECK(!Valve_ClearFaults(now, &input));
  step(1900); step(20); step(2);
  CHECK(output == VALVE_DRIVE_RUN && direction == VALVE_DIR_CLOSE); tests_passed++;
  return 0;
}
