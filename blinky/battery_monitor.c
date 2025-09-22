/*******************************************************************************
 * @file  battery_monitor.c
 * @brief Battery monitoring and charging detection implementation
 *******************************************************************************
 */

#include "battery_monitor.h"
#include "app_log.h"
#include "sl_status.h"
#include "cmsis_os2.h"

// Global state
static battery_status_t g_battery_status = {0};
static battery_state_callback_t g_state_callback = NULL;
static osTimerId_t g_monitor_timer = NULL;
static bool g_monitor_active = false;

// Timer period for battery monitoring (in ms)
#define BATTERY_MONITOR_PERIOD_MS 5000  // Check every 5 seconds

// Forward declarations
static void battery_monitor_timer_callback(void *argument);
static void battery_update_state(void);
static void battery_handle_state_change(battery_state_t old_state, battery_state_t new_state);

sl_status_t battery_monitor_init(void)
{
    // Initialize battery status
    g_battery_status.battery_percent = 100;
    g_battery_status.battery_voltage = 4.2f;
    g_battery_status.charging_current_ma = 0.0f;
    g_battery_status.state = BATTERY_STATE_NORMAL;
    g_battery_status.is_charging = false;
    g_battery_status.is_low_battery_warning_active = false;

    // Create timer for periodic battery monitoring
    osTimerAttr_t timer_attr = {
        .name = "battery_monitor_timer"
    };

    g_monitor_timer = osTimerNew(battery_monitor_timer_callback, osTimerPeriodic, NULL, &timer_attr);
    if (g_monitor_timer == NULL) {
        app_log_error("Failed to create battery monitor timer\r\n");
        return SL_STATUS_FAIL;
    }

    app_log_info("Battery monitor initialized\r\n");
    return SL_STATUS_OK;
}

sl_status_t battery_monitor_start(void)
{
    if (g_monitor_timer == NULL) {
        return SL_STATUS_NOT_INITIALIZED;
    }

    osStatus_t status = osTimerStart(g_monitor_timer, BATTERY_MONITOR_PERIOD_MS);
    if (status != osOK) {
        app_log_error("Failed to start battery monitor timer\r\n");
        return SL_STATUS_FAIL;
    }

    g_monitor_active = true;
    app_log_info("Battery monitor started\r\n");
    return SL_STATUS_OK;
}

void battery_monitor_stop(void)
{
    if (g_monitor_timer != NULL) {
        osTimerStop(g_monitor_timer);
    }

    // Stop low battery warning if active
    if (g_battery_status.is_low_battery_warning_active) {
        g_battery_status.is_low_battery_warning_active = false;
    }

    g_monitor_active = false;
    app_log_info("Battery monitor stopped\r\n");
}

sl_status_t battery_monitor_set_callback(battery_state_callback_t callback)
{
    g_state_callback = callback;
    return SL_STATUS_OK;
}

battery_status_t battery_monitor_get_status(void)
{
    return g_battery_status;
}

void battery_monitor_force_update(void)
{
    battery_update_state();
}

void battery_monitor_update_values(uint8_t battery_percent, float battery_voltage, float charging_current_ma)
{
    battery_state_t old_state = g_battery_status.state;

    g_battery_status.battery_percent = battery_percent;
    g_battery_status.battery_voltage = battery_voltage;
    g_battery_status.charging_current_ma = charging_current_ma;

    battery_update_state();

    if (g_battery_status.state != old_state) {
        battery_handle_state_change(old_state, g_battery_status.state);
    }
}

static void battery_monitor_timer_callback(void *argument)
{
    (void)argument;

    if (!g_monitor_active) {
        return;
    }

    // Get current battery information (using stub data for now)
    // In a real implementation, this would read from the PMIC or ADC

    // For now, use simulated values (this should be replaced with actual hardware readings)
    uint8_t battery_pct = 85;  // Simulate normal battery level
    float battery_v = 3.8f;    // Simulate battery voltage
    float charging_a = 0.0f;   // Simulate no charging (in Amps)

    battery_monitor_update_values(battery_pct, battery_v, charging_a * 1000.0f);
}

static void battery_update_state(void)
{
    battery_state_t new_state;
    bool is_charging = (g_battery_status.charging_current_ma > CHARGING_CURRENT_THRESHOLD_MA);

    g_battery_status.is_charging = is_charging;

    if (is_charging) {
        if (g_battery_status.battery_percent >= BATTERY_FULL_THRESHOLD_PERCENT) {
            new_state = BATTERY_STATE_FULL;
        } else {
            new_state = BATTERY_STATE_CHARGING;
        }
    } else {
        if (g_battery_status.battery_percent <= BATTERY_CRITICAL_THRESHOLD_PERCENT) {
            new_state = BATTERY_STATE_CRITICAL;
        } else if (g_battery_status.battery_percent <= BATTERY_LOW_THRESHOLD_PERCENT) {
            new_state = BATTERY_STATE_LOW;
        } else {
            new_state = BATTERY_STATE_NORMAL;
        }
    }

    g_battery_status.state = new_state;
}

static void battery_handle_state_change(battery_state_t old_state, battery_state_t new_state)
{
    app_log_info("Battery state changed: %d -> %d (%.1f%%, %.2fV, %.1fmA)\r\n",
                 old_state, new_state,
                 g_battery_status.battery_percent,
                 g_battery_status.battery_voltage,
                 g_battery_status.charging_current_ma);

    // Battery state changes logged only (LED control removed)
    switch (new_state) {
        case BATTERY_STATE_LOW:
        case BATTERY_STATE_CRITICAL:
            if (!g_battery_status.is_low_battery_warning_active) {
                g_battery_status.is_low_battery_warning_active = true;
                app_log_warning("Low battery warning activated\r\n");
            }
            break;

        case BATTERY_STATE_CHARGING:
            if (g_battery_status.is_low_battery_warning_active) {
                g_battery_status.is_low_battery_warning_active = false;
            }
            app_log_info("Charging started\r\n");
            break;

        case BATTERY_STATE_FULL:
            if (g_battery_status.is_low_battery_warning_active) {
                g_battery_status.is_low_battery_warning_active = false;
            }
            app_log_info("Charging complete\r\n");
            break;

        case BATTERY_STATE_NORMAL:
            if (g_battery_status.is_low_battery_warning_active) {
                g_battery_status.is_low_battery_warning_active = false;
                app_log_info("Battery level normal - low battery warning stopped\r\n");
            }
            break;

        default:
            break;
    }

    // Call user callback if registered
    if (g_state_callback) {
        g_state_callback(old_state, new_state, &g_battery_status);
    }
}

// Simulation functions for testing
void battery_monitor_simulate_low_battery(void)
{
    app_log_info("Simulating low battery condition\r\n");
    battery_monitor_update_values(15, 3.2f, 0.0f);  // 15% battery, no charging
}

void battery_monitor_simulate_charging_start(void)
{
    app_log_info("Simulating charging start\r\n");
    battery_monitor_update_values(30, 3.5f, 500.0f);  // 30% battery, 500mA charging
}

void battery_monitor_simulate_charging_complete(void)
{
    app_log_info("Simulating charging complete\r\n");
    battery_monitor_update_values(100, 4.2f, 20.0f);  // 100% battery, trickle charge
}