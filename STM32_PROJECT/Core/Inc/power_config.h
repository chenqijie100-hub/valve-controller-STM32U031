#ifndef POWER_CONFIG_H
#define POWER_CONFIG_H
/* 0: ASCII debug, original SysTick; 1: stage3A Sleep; 2: stage3B Stop1.
 * Stop1 wakes on GPIO only, not UART; background ADC pauses until wake. */
#ifndef POWER_LOW_POWER_MODE
#define POWER_LOW_POWER_MODE 0
#endif
#if POWER_LOW_POWER_MODE < 0 || POWER_LOW_POWER_MODE > 2
#error POWER_LOW_POWER_MODE must be 0, 1 or 2
#endif
#define POWER_MAX_SLEEP_MS 250U
#endif
