#ifndef POWER_CONFIG_H
#define POWER_CONFIG_H
/* Default is ASCII debug mode: no automatic sleep, original SysTick timebase.
 * Set to1 for stage3A ordinary Sleep. CMake exposes VALVE_LOW_POWER=ON/OFF. */
#ifndef POWER_LOW_POWER_MODE
#define POWER_LOW_POWER_MODE 0
#endif
#if POWER_LOW_POWER_MODE != 0 && POWER_LOW_POWER_MODE != 1
#error POWER_LOW_POWER_MODE must be 0 or 1
#endif
#define POWER_MAX_SLEEP_MS 250U
#endif
