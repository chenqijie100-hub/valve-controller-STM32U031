#ifndef VALVE_CONFIG_H
#define VALVE_CONFIG_H

/* Normal and safety-close motion have no maximum runtime: stop at the
 * destination limit or a protection fault. Explicit jog remains 100 ms. */
#define VALVE_INPUT_STABLE_MS     50U
#define VALVE_WIRE_RECOVERY_MS   100U
/* Retained from the previously board-tested drive sequence. */
#define VALVE_REVERSAL_WAIT_MS  2000U
#define VALVE_SLEEP_SETTLE_MS      2U
#define VALVE_VREF_SETTLE_MS      20U
#define VALVE_WAKE_SETTLE_MS       2U

#endif
