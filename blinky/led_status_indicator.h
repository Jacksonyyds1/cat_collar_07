/*******************************************************************************
 * @file  led_status_indicator.h
 * @brief Blue LED status indicator system for cat collar device states
 *******************************************************************************
 * # License
 * <b>Copyright 2024 Silicon Laboratories Inc. www.silabs.com</b>
 *******************************************************************************
 */

#ifndef LED_STATUS_INDICATOR_H
#define LED_STATUS_INDICATOR_H

#include <stdint.h>
#include <stdbool.h>
#include "cmsis_os2.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    LED_STATUS_POWER_ON = 0,              // Blue solid for 2 seconds
    LED_STATUS_BLE_PAIRING,               // Blue flash every 1 second
    LED_STATUS_BLE_PAIR_SUCCESS,          // Blue solid for 2 seconds
    LED_STATUS_BLE_PAIR_FAIL,             // Blue double flash, pause 1s, double flash, continue
    LED_STATUS_FACTORY_RESET,             // Blue solid for 2 seconds
    LED_STATUS_LOW_BATTERY,               // Blue breathing pattern
    LED_STATUS_OTA_UPDATE,                // Blue flash every 3 seconds
    LED_STATUS_OTA_SUCCESS,               // Blue off
    LED_STATUS_OTA_FAIL,                  // Blue fast flash every 3 seconds
    LED_STATUS_CHARGING,                  // Blue solid during charging
    LED_STATUS_CHARGE_COMPLETE,           // Blue off when fully charged
    LED_STATUS_OFF,                       // LED off
    LED_STATUS_MAX
} led_status_t;

typedef enum {
    LED_PATTERN_STATE_IDLE = 0,
    LED_PATTERN_STATE_ACTIVE,
    LED_PATTERN_STATE_COMPLETE
} led_pattern_state_t;

typedef struct {
    uint16_t on_time_ms;     // Time LED stays on (in ms)
    uint16_t off_time_ms;    // Time LED stays off (in ms)
    uint8_t repeat_count;    // Number of times to repeat (0 = infinite)
    uint8_t flash_count;     // Number of flashes in this step
} led_pattern_step_t;

typedef struct {
    const led_pattern_step_t *steps;
    uint8_t step_count;
    bool auto_stop;          // True if pattern should auto-stop after completion
    uint16_t auto_stop_delay_ms; // Delay before auto-stopping
} led_pattern_config_t;

typedef struct {
    led_status_t current_status;
    led_pattern_state_t pattern_state;
    uint8_t current_step;
    uint8_t current_flash;
    uint8_t repeat_counter;
    uint32_t step_start_time;
    uint32_t pattern_start_time;
    bool led_state;
    osTimerHandle_t timer_handle;
    osTimerHandle_t auto_stop_timer;
} led_status_context_t;

// Function declarations
sl_status_t led_status_indicator_init(void);
sl_status_t led_status_set(led_status_t status);
led_status_t led_status_get_current(void);
bool led_status_is_active(void);
void led_status_stop(void);

// Convenience functions for common use cases
void led_status_power_on(void);
void led_status_ble_enter_pairing(void);
void led_status_ble_pairing_success(void);
void led_status_ble_pairing_failed(void);
void led_status_factory_reset(void);
void led_status_low_battery_warning(void);
void led_status_charging_started(void);
void led_status_charging_complete(void);
void led_status_ota_update_start(void);
void led_status_ota_update_success(void);
void led_status_ota_update_failed(void);

// Breathing pattern control
void led_status_stop_low_battery_warning(void);

#ifdef __cplusplus
}
#endif

#endif // LED_STATUS_INDICATOR_H