/*******************************************************************************
 * @file  led_status_indicator.c
 * @brief Blue LED status indicator system implementation
 *******************************************************************************
 */

#include "led_status_indicator.h"
#include "blinky.h"
#include "app_log.h"
#include "sl_status.h"
#include "cmsis_os2.h"

// LED pattern definitions for each status
static const led_pattern_step_t pattern_power_on[] = {
    {2000, 0, 1, 1}  // On for 2 seconds, then stop
};

static const led_pattern_step_t pattern_ble_pairing[] = {
    {200, 800, 0, 1}  // Flash for 200ms, off for 800ms, repeat infinitely
};

static const led_pattern_step_t pattern_ble_pair_success[] = {
    {2000, 0, 1, 1}  // On for 2 seconds, then stop
};

static const led_pattern_step_t pattern_ble_pair_fail[] = {
    {200, 200, 1, 2},  // Double flash: 200ms on, 200ms off, 200ms on, 200ms off
    {0, 600, 1, 1},    // Pause for 600ms more (total 1s pause)
    {200, 200, 0, 2}   // Repeat double flash infinitely
};

static const led_pattern_step_t pattern_factory_reset[] = {
    {2000, 0, 1, 1}  // On for 2 seconds, then stop
};


static const led_pattern_step_t pattern_ota_update[] = {
    {200, 2800, 0, 1}  // Flash for 200ms every 3 seconds
};

static const led_pattern_step_t pattern_ota_fail[] = {
    {100, 100, 1, 3},  // Triple fast flash
    {0, 2700, 1, 1}    // Pause, then repeat every 3 seconds
};


static const led_pattern_step_t pattern_off[] = {
    {0, 0, 0, 0}  // Special case: off
};

// Pattern configuration table
static const led_pattern_config_t led_patterns[LED_STATUS_MAX] = {
    [LED_STATUS_POWER_ON] = {pattern_power_on, 1, true, 0},
    [LED_STATUS_BLE_PAIRING] = {pattern_ble_pairing, 1, false, 0},
    [LED_STATUS_BLE_PAIR_SUCCESS] = {pattern_ble_pair_success, 1, true, 0},
    [LED_STATUS_BLE_PAIR_FAIL] = {pattern_ble_pair_fail, 3, false, 0},
    [LED_STATUS_FACTORY_RESET] = {pattern_factory_reset, 1, true, 0},
    [LED_STATUS_OTA_UPDATE] = {pattern_ota_update, 1, false, 0},
    [LED_STATUS_OTA_SUCCESS] = {pattern_off, 1, true, 0},
    [LED_STATUS_OTA_FAIL] = {pattern_ota_fail, 2, false, 0},
    [LED_STATUS_OFF] = {pattern_off, 1, true, 0}
};

// Global context
static led_status_context_t g_led_context = {0};

// Timer callback forward declarations
static void led_timer_callback(void *argument);
static void auto_stop_timer_callback(void *argument);

// Internal functions
static void led_set_physical_state(bool on);
static void led_start_next_step(void);
static void led_process_current_step(void);

sl_status_t led_status_indicator_init(void)
{
    // Initialize the LED context
    g_led_context.current_status = LED_STATUS_OFF;
    g_led_context.pattern_state = LED_PATTERN_STATE_IDLE;
    g_led_context.current_step = 0;
    g_led_context.current_flash = 0;
    g_led_context.repeat_counter = 0;
    g_led_context.led_state = false;

    // Create timer for LED pattern control
    osTimerAttr_t timer_attr = {
        .name = "led_status_timer"
    };

    g_led_context.timer_handle = osTimerNew(led_timer_callback, osTimerOnce, NULL, &timer_attr);
    if (g_led_context.timer_handle == NULL) {
        app_log_error("Failed to create LED status timer\r\n");
        return SL_STATUS_FAIL;
    }

    // Create auto-stop timer
    osTimerAttr_t auto_stop_attr = {
        .name = "led_auto_stop_timer"
    };

    g_led_context.auto_stop_timer = osTimerNew(auto_stop_timer_callback, osTimerOnce, NULL, &auto_stop_attr);
    if (g_led_context.auto_stop_timer == NULL) {
        app_log_error("Failed to create LED auto-stop timer\r\n");
        return SL_STATUS_FAIL;
    }

    // Make sure LED is off initially
    led_set_physical_state(false);

    app_log_debug("LED status indicator initialized\r\n");
    return SL_STATUS_OK;
}

sl_status_t led_status_set(led_status_t status)
{
    if (status >= LED_STATUS_MAX) {
        return SL_STATUS_INVALID_PARAMETER;
    }

    // Stop any current pattern
    osTimerStop(g_led_context.timer_handle);
    osTimerStop(g_led_context.auto_stop_timer);

    // Update context
    g_led_context.current_status = status;
    g_led_context.pattern_state = LED_PATTERN_STATE_ACTIVE;
    g_led_context.current_step = 0;
    g_led_context.current_flash = 0;
    g_led_context.repeat_counter = 0;
    g_led_context.pattern_start_time = osKernelGetTickCount();

    app_log_debug("LED status set to: %d\r\n", status);

    // Handle special cases
    if (status == LED_STATUS_OFF || status == LED_STATUS_OTA_SUCCESS) {
        led_set_physical_state(false);
        g_led_context.pattern_state = LED_PATTERN_STATE_COMPLETE;
        return SL_STATUS_OK;
    }

    // Start the pattern
    led_start_next_step();

    return SL_STATUS_OK;
}

led_status_t led_status_get_current(void)
{
    return g_led_context.current_status;
}

bool led_status_is_active(void)
{
    return g_led_context.pattern_state == LED_PATTERN_STATE_ACTIVE;
}

void led_status_stop(void)
{
    osTimerStop(g_led_context.timer_handle);
    osTimerStop(g_led_context.auto_stop_timer);
    led_set_physical_state(false);
    g_led_context.pattern_state = LED_PATTERN_STATE_IDLE;
    g_led_context.current_status = LED_STATUS_OFF;
}

static void led_set_physical_state(bool on)
{
    g_led_context.led_state = on;
    if (on) {
        leds_play(BLUE_LED, LEDS_ON);
    } else {
        leds_play(BLUE_LED, LEDS_OFF);
    }
}

static void led_start_next_step(void)
{
    const led_pattern_config_t *config = &led_patterns[g_led_context.current_status];

    if (g_led_context.current_step >= config->step_count) {
        // Pattern complete, check for repeat
        const led_pattern_step_t *first_step = &config->steps[0];
        if (first_step->repeat_count == 0) {
            // Infinite repeat, restart from beginning
            g_led_context.current_step = 0;
        } else {
            // Pattern finished
            g_led_context.pattern_state = LED_PATTERN_STATE_COMPLETE;
            led_set_physical_state(false);

            if (config->auto_stop && config->auto_stop_delay_ms > 0) {
                osTimerStart(g_led_context.auto_stop_timer, config->auto_stop_delay_ms);
            }
            return;
        }
    }

    g_led_context.current_flash = 0;
    g_led_context.step_start_time = osKernelGetTickCount();

    led_process_current_step();
}

static void led_process_current_step(void)
{
    const led_pattern_config_t *config = &led_patterns[g_led_context.current_status];
    const led_pattern_step_t *step = &config->steps[g_led_context.current_step];

    if (g_led_context.current_flash >= step->flash_count && step->flash_count > 0) {
        // Move to next step
        g_led_context.current_step++;
        led_start_next_step();
        return;
    }

    // Determine next state and timing
    bool next_led_state;
    uint32_t next_delay;

    if (step->on_time_ms == 0 && step->off_time_ms == 0) {
        // Special case: solid state
        next_led_state = false;
        next_delay = 1000; // Check again in 1 second
    } else {
        // Normal flash pattern
        if (!g_led_context.led_state) {
            // LED is off, turn it on
            next_led_state = true;
            next_delay = step->on_time_ms;
        } else {
            // LED is on, turn it off
            next_led_state = false;
            next_delay = step->off_time_ms;
            g_led_context.current_flash++;
        }
    }

    led_set_physical_state(next_led_state);

    if (next_delay > 0) {
        osTimerStart(g_led_context.timer_handle, next_delay);
    } else {
        // Continue immediately
        led_process_current_step();
    }
}

static void led_timer_callback(void *argument)
{
    (void)argument;

    if (g_led_context.pattern_state == LED_PATTERN_STATE_ACTIVE) {
        led_process_current_step();
    }
}

static void auto_stop_timer_callback(void *argument)
{
    (void)argument;
    led_status_stop();
}

// Convenience functions
void led_status_power_on(void)
{
    led_status_set(LED_STATUS_POWER_ON);
}

void led_status_ble_enter_pairing(void)
{
    led_status_set(LED_STATUS_BLE_PAIRING);
}

void led_status_ble_pairing_success(void)
{
    led_status_set(LED_STATUS_BLE_PAIR_SUCCESS);
}

void led_status_ble_pairing_failed(void)
{
    led_status_set(LED_STATUS_BLE_PAIR_FAIL);
}

void led_status_factory_reset(void)
{
    led_status_set(LED_STATUS_FACTORY_RESET);
}


void led_status_ota_update_start(void)
{
    led_status_set(LED_STATUS_OTA_UPDATE);
}

void led_status_ota_update_success(void)
{
    led_status_set(LED_STATUS_OTA_SUCCESS);
}

void led_status_ota_update_failed(void)
{
    led_status_set(LED_STATUS_OTA_FAIL);
}

