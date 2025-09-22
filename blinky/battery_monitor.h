/*******************************************************************************
 * @file  battery_monitor.h
 * @brief Battery monitoring and charging detection with LED status integration
 *******************************************************************************
 */

#ifndef BATTERY_MONITOR_H
#define BATTERY_MONITOR_H

#include <stdint.h>
#include <stdbool.h>
#include "sl_status.h"

#ifdef __cplusplus
extern "C" {
#endif

// Battery threshold levels
#define BATTERY_LOW_THRESHOLD_PERCENT       20  // Low battery warning at 20%
#define BATTERY_CRITICAL_THRESHOLD_PERCENT  10  // Critical battery at 10%
#define BATTERY_FULL_THRESHOLD_PERCENT      95  // Consider full at 95%

// Charging current threshold (in mA)
#define CHARGING_CURRENT_THRESHOLD_MA       50  // Minimum current to consider charging

typedef enum {
    BATTERY_STATE_UNKNOWN = 0,
    BATTERY_STATE_NORMAL,
    BATTERY_STATE_LOW,
    BATTERY_STATE_CRITICAL,
    BATTERY_STATE_CHARGING,
    BATTERY_STATE_FULL
} battery_state_t;

typedef struct {
    uint8_t battery_percent;
    float battery_voltage;
    float charging_current_ma;
    battery_state_t state;
    bool is_charging;
    bool is_low_battery_warning_active;
} battery_status_t;

// Callback function type for battery state changes
typedef void (*battery_state_callback_t)(battery_state_t old_state, battery_state_t new_state, const battery_status_t* status);

// Function declarations
sl_status_t battery_monitor_init(void);
sl_status_t battery_monitor_start(void);
void battery_monitor_stop(void);
sl_status_t battery_monitor_set_callback(battery_state_callback_t callback);
battery_status_t battery_monitor_get_status(void);
void battery_monitor_force_update(void);

// Manual update functions (for testing or external triggers)
void battery_monitor_update_values(uint8_t battery_percent, float battery_voltage, float charging_current_ma);
void battery_monitor_simulate_low_battery(void);
void battery_monitor_simulate_charging_start(void);
void battery_monitor_simulate_charging_complete(void);

#ifdef __cplusplus
}
#endif

#endif // BATTERY_MONITOR_H