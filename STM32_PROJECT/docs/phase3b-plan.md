# 3B Stop 1 implementation plan

Goal: add GPIO-woken Stop 1 behind an explicit profile while preserving the tested valve state machine and 3A Sleep.

User specification: 2026-10-29 stage3B request in this conversation. Execution is authorized here; no Push, no Stop2/Standby, no RTC/UART wake, no LIMIT_EN or motor timing changes.

Design decisions:
- Profiles 0 debug (default), 1 existing Sleep, 2 Stop1. Current HSI16 SYSCLK, AHB/APB div1 only; other configurations refuse Stop.
- Reuse the existing generation/signature/PRIMASK entry gate. Never clear GPIO EXTI pending to make entry succeed. Restore clocks and time with IRQs still masked, then return to fresh safety sampling before ADC/UART processing.
- RTC already runs from LSE. Read its calendar/subseconds only, with no alarm/wakeup timer. Account for elapsed Stop time in the TIM2-based HAL millisecond clock. Frozen TIM2 must never be used for a HAL timeout during restoration; use bounded register checks.
- Stop suspends periodic background battery sampling until an external wake. Old data retains its timestamp and becomes invalid with age; next safety service precedes resumed ADC sampling. No periodic timer is promised during Stop.
- USART1 disabled across Stop, restored afterwards; no UART wake. Discard the interrupted stream up to a newline after resumption so a tail cannot execute as a command. Debug profile retains original ASCII operation.
- Entry still requires no motor, safety close, debounce, reversal, ADC, UART or unprocessed event. Keep PWM0/nSLEEP0/VREF0 and LIMIT_EN low; verify rather than change their policy.

Tasks (test-first, native execution):
1. Add executable calendar/time tests and Stop port tests (pending events before/at WFI, long sleep/day rollover, clock/RTC failures, UART busy and resynchronization); confirm failures before implementation.
2. Implement bounded RTC reads and Stop adapter, profile/build links and UART resynchronization. Preserve valve_control/valve_port and ADC conversion code.
3. Run existing and added emulator suites in all three profiles; perform fresh debug/Sleep/Stop builds. Inspect diff, protected-file hashes and clock/HAL integration.
4. Document exact source references, paths, diagnostics, all modified files and practical board acceptance (100+ cycles, safety wake, restored clocks, nSLEEP probing, latency and current).

Review focus: late safety pulses must remain pending; stopped tick cannot hang recovery; long idle must age ADC values; corrupt/mid-frame UART must not execute; failed restoration must keep motor off. Host tests cannot validate physical Stop entry, wake latency, silicon errata applicability or current.
