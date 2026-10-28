# Phase 1: valve state machine and wire-break safety

**2026-10-28 superseding requirement:** The user explicitly removed maximum opening/closing runtime. Normal and safety-close motion now run until the destination limit or another protection/valid control stop; no runtime timeout is generated. Explicit 100 ms jog remains. Timeout-related design/tasks below describe the original 2026-10-27 implementation only. Current behavior and acceptance criteria are in phase1-acceptance.md.

The user's 2026-10-27 hardware definitions and implementation requirements are the specification and authorization for this phase. No STOP mode, RTOS, Modbus, battery percentage, or unvalidated current threshold is included.

## Baseline

Git reports `No commits yet on master`; all existing project files are untracked. No reset, checkout, staging or commit is performed. The pre-edit source/config/library snapshot (1231 files), hashes and initial Git status are in `../../tmp/phase1-baseline-20261027/`. Existing CMake HAL source additions are retained.

## Design and decisions

Use a portable `valve_control.c/.h` state machine with a thin `valve_port.c/.h` adapter. Keep the proven PH mapping, PWM calculation and output levels. Replace blocking wake delays with timed phases (sleep >=2 ms, reference >=20 ms, wake >=2 ms); retain a full 2000 ms stop/reversal interval. Main owns state and faults; the LIMIT4 rising ISR only records a sticky event and immediately disables an opening/preparing drive. Every energizing write is guarded again inside a short interrupt-masked critical section, checking the raw pin, EXTI pending flag and sticky event.

Six public states: UNKNOWN, OPENING, OPENED, CLOSING, CLOSED, FAULT. Startup is disarmed until all peripherals/calibration/settling complete. Digital control then defaults on. Wire break asserts immediately, recovers after 100 ms continuously low, and creates an irrevocable close request until exclusive close-limit confirmation. Opening stays inhibited until a new debounced low observation after recovery (a low held through recovery must be observed for a fresh 50 ms). A subsequent high permits opening. A held break must not repeatedly reset the close sequencer.

Current faults, historical faults and latched protective faults are distinct. Wire break recovers automatically. Timeout, awake-driver fault, contradictory limits and driver-start failure require explicit FAULTCLR with drive off, raw wire normal, nFAULT healthy and limits noncontradictory. History survives FAULTCLR in RAM; power loss clears it. Safety-close failure is reported separately and never treated as CLOSED. FAULTCLR preserves/retries a pending close; no automatic protective retries. STOP, DINOFF, SLEEP and manual commands cannot cancel safety close. Debug PWM/VREF/WAKE commands are rejected in this phase, retaining named ASCII replies rather than parallel output ownership. JOG/RUN use the same arbiter and cannot bypass the post-break low interlock.

Temporary OPEN/CLOSE limits: 30000 ms each, clearly labeled NOT validated. They bound EN-active motion including jog; jog normally ends at 100 ms without a timeout fault. Direction requests are debounced 50 ms. Limit bounce does not cause automatic restart after a destination stop. Driver nFAULT is checked after wake and while energized, since sleep-time indication is not used to claim a drive fault; reset requires the pin healthy.

ASCII STATUS/PING are retained; no formal protocol redesign. Transmit remains polling but services safety between bytes and during waits. ADC polls use zero timeout in a cooperatively serviced wait. Communication errors discard the damaged line without cancelling safety close. No state transitions or replies occur in EXTI. Fatal handlers use a HAL/tick-independent direct-register output-off routine; this cannot guarantee mechanical closure, survive a stalled CPU or missing power, or persist fault history.

## Tasks and verification

- [x] Write executable tests before implementation; prove a controller stub fails startup control and a port stub fails reference output assertions.
- [x] Add portable controller and fault/recovery policy; exercise all six states, startup, break at every wake phase, sticky closure, recovery low/high interlock, destination bounce, timeout, simultaneous faults, tick rollover, manual/debug arbitration.
- [x] Add STM32 adapter, output guard/ISR/fatal off; test real adapter with mock HAL and injected interrupts at every enable boundary.
- [x] Integrate startup, ASCII diagnostics, cooperative TX/ADC and exception handlers; test STATUS/PING and damaged input during closing using actual main.c.
- [x] Add CMake and CubeIDE source entries; clean configure/build in a new directory; retain baseline scripts and unrelated files in the snapshot. Migrate the old test entry point to the new complete suite.
- [x] Review full diff, rerun executable suite, document exact commands, board acceptance checklist, provisional timing and software limits.

## Verification rulings

- Native Windows GCC was unavailable; compile the real C for Cortex-M0+ with the installed ARM compiler and execute on host Unicorn 2.1.4. Board GPIO/HAL are mocked; application formatting uses real newlib. The dependency is test-only, outside firmware.
- Application integration was checked by executable tests after wiring it in; controller and port each had an observed failing behavior assertion before implementation. Additional deadline and no-fault FAULTCLR regressions were observed red and then green.
- A delayed arrival sample must not erase a maximum-runtime violation; collect timeout alongside other protection faults before destination handling.
- FAULTCLR with no protection latch is refused, because resetting cooldown on each such command could otherwise starve a pending safety close.
- Explicit manual open after a qualified post-recovery low can substitute for a digital high request. It cannot bypass that low qualification.
- Independent read-only code review confirmed the timeout fix and no-fault clear guard, and reran 19 controller + 14 adapter + 23 application scenarios successfully.

No extra design approval round is requested: the user supplied a detailed design and explicitly requested implementation. Work remains in the shared workspace with a separate immutable source snapshot, rather than relocating or discarding untracked user files.
