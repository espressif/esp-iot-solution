/*
 * SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: CC0-1.0
 */

/*
 * LVGL Light Sleep Demo - shows the esp_lvgl_adapter auto-sleep mechanism.
 *
 * Two auto-sleep modes, chosen by the display interface:
 *
 *   PAUSE (SPI/QSPI): the adapter pauses the LVGL worker and releases its PM
 *   lock; the system enters tickless light sleep with the panel kept alive.
 *   If the touch panel has an INT pin, it is armed as a GPIO wakeup source so
 *   a finger tap wakes the MCU immediately; a one-shot timer provides a
 *   guaranteed auto-wake regardless.  The adapter's built-in touch ISR wrapper
 *   calls esp_lv_adapter_request_wake_from_isr(), so no extra LVGL-side code
 *   is needed.
 *
 *   USER (RGB/MIPI): on idle the adapter calls on_enter_sleep, which runs the
 *   full cycle synchronously:
 *       sleep_prepare -> hw_lcd_deinit -> esp_light_sleep_start
 *       -> hw_lcd_init -> sleep_recover
 *   Touch INT (if present) and a timer are both armed before the sleep call.
 *
 * Boundary: the adapter decides when LVGL may sleep/wake; the application owns
 * all LCD/panel operations and the choice of wake source.
 */

#include <inttypes.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_check.h"
#include "esp_err.h"
#include "esp_log.h"
#include "esp_pm.h"
#include "esp_sleep.h"
#include "esp_timer.h"
#include "hw_init.h"
#include "esp_lv_adapter.h"
#include "lvgl.h"

static const char *TAG = "main";

static lv_display_t *s_disp;
static lv_obj_t *s_status_label;
static lv_obj_t *s_action_button;
static lv_indev_t *s_encoder;
#if HW_USE_TOUCH
static lv_indev_t *s_touch_indev;
#endif
static esp_lcd_panel_handle_t s_panel_handle;
static esp_lcd_panel_io_handle_t s_panel_io_handle;
#if HW_USE_TOUCH
static esp_lcd_touch_handle_t s_touch_handle;
#endif
static esp_lv_adapter_rotation_t s_rotation;
static esp_lv_adapter_tear_avoid_mode_t s_tear_mode;
static uint32_t s_activity_count;
static uint32_t s_user_sleep_cycles;
static uint64_t s_last_sleep_duration_us;
static uint32_t s_last_wake_causes;
static esp_timer_handle_t s_pause_wake_timer;

static void create_demo_ui(void);
static void action_button_event_cb(lv_event_t *e);
static void bind_encoder_to_ui(void);
static void update_status_label(void);
static esp_err_t validate_auto_sleep_platform_config(void);
static esp_err_t configure_pm_for_auto_sleep(void);
static esp_err_t configure_adapter_auto_sleep(esp_lv_adapter_config_t *adapter_config);
static esp_err_t example_unregister_touch_input(void);
static esp_err_t example_register_touch_input(void);
static esp_err_t recover_failed_light_sleep_cycle(bool panel_handles_valid, bool touch_registered);
static esp_err_t run_full_light_sleep_cycle(void);
static esp_err_t example_panel_set_sleep(bool sleep);
static void pause_wake_timer_cb(void *arg);
static esp_err_t pause_wake_timer_ensure(void);
static esp_err_t pause_mode_enter_sleep(void *user_ctx);
static esp_err_t pause_mode_exit_sleep(void *user_ctx);
static esp_err_t user_mode_enter_sleep(void *user_ctx);
static gpio_num_t example_touch_int_gpio(void);
static bool example_uses_auto_sleep(void);
static bool example_uses_manual_sleep(void);
static bool example_uses_pause_auto_sleep(void);
static bool example_use_compact_ui(void);
static uint32_t example_get_auto_sleep_idle_timeout_ms(void);
static const char *example_get_auto_sleep_mode_name(void);
static const char *example_get_action_button_text(void);
static const char *example_get_sleep_hint_text(void);
static const char *example_get_wake_hint_text(void);
static const lv_font_t *example_get_status_font(void);
static const lv_font_t *example_get_button_font(void);
static lv_coord_t example_get_status_top_offset(void);
static lv_coord_t example_get_status_line_space(void);
static lv_coord_t example_get_button_height(void);
static lv_coord_t example_get_button_bottom_offset(void);
static lv_coord_t example_get_button_radius(void);
static esp_lv_adapter_rotation_t get_configured_rotation(void);
static esp_lv_adapter_tear_avoid_mode_t get_default_tear_mode(void);

static bool example_uses_auto_sleep(void)
{
#if CONFIG_EXAMPLE_SLEEP_MODE_AUTO
    return true;
#else
    return false;
#endif
}

static bool example_uses_manual_sleep(void)
{
    return !example_uses_auto_sleep();
}

static bool example_uses_pause_auto_sleep(void)
{
    if (!example_uses_auto_sleep()) {
        return false;
    }

    /* SPI/QSPI keep their own GRAM -> pause mode; RGB/MIPI need full teardown -> user mode */
#if CONFIG_EXAMPLE_LCD_INTERFACE_RGB || CONFIG_EXAMPLE_LCD_INTERFACE_MIPI_DSI
    return false;
#else
    return true;
#endif
}

static bool example_use_compact_ui(void)
{
    return (HW_LCD_H_RES <= 320) || (HW_LCD_V_RES <= 240);
}

static uint32_t example_get_auto_sleep_idle_timeout_ms(void)
{
#if CONFIG_EXAMPLE_SLEEP_MODE_AUTO
    return CONFIG_EXAMPLE_AUTO_SLEEP_IDLE_TIMEOUT_MS;
#else
    return 0;
#endif
}

static const char *example_get_auto_sleep_mode_name(void)
{
    if (example_uses_manual_sleep()) {
        return "manual light sleep";
    }

    return example_uses_pause_auto_sleep() ? "adapter pause + PM" : "user managed light sleep";
}

static const char *example_get_action_button_text(void)
{
    return example_uses_manual_sleep() ? "Sleep Now" : "Stay Awake";
}

static const char *example_get_sleep_hint_text(void)
{
    if (example_uses_manual_sleep()) {
        return "Sleep: manual button";
    }

    return "Sleep: auto after idle";
}

static const char *example_get_wake_hint_text(void)
{
    if (example_touch_int_gpio() != GPIO_NUM_NC) {
        return "Wake: touch or timer";
    }

    return "Wake: timer only";
}

static const lv_font_t *example_get_status_font(void)
{
    return example_use_compact_ui() ? LV_FONT_DEFAULT : &lv_font_montserrat_20;
}

static const lv_font_t *example_get_button_font(void)
{
    return example_use_compact_ui() ? &lv_font_montserrat_20 : &lv_font_montserrat_24;
}

static lv_coord_t example_get_status_top_offset(void)
{
    return example_use_compact_ui() ? 14 : 36;
}

static lv_coord_t example_get_status_line_space(void)
{
    return example_use_compact_ui() ? 4 : 10;
}

static lv_coord_t example_get_button_height(void)
{
    return example_use_compact_ui() ? 56 : 72;
}

static lv_coord_t example_get_button_bottom_offset(void)
{
    return example_use_compact_ui() ? -12 : -28;
}

static lv_coord_t example_get_button_radius(void)
{
    return example_use_compact_ui() ? 14 : 18;
}

static esp_lv_adapter_rotation_t get_configured_rotation(void)
{
#if CONFIG_EXAMPLE_DISPLAY_ROTATION_0
    return ESP_LV_ADAPTER_ROTATE_0;
#elif CONFIG_EXAMPLE_DISPLAY_ROTATION_90
    return ESP_LV_ADAPTER_ROTATE_90;
#elif CONFIG_EXAMPLE_DISPLAY_ROTATION_180
    return ESP_LV_ADAPTER_ROTATE_180;
#elif CONFIG_EXAMPLE_DISPLAY_ROTATION_270
    return ESP_LV_ADAPTER_ROTATE_270;
#else
    return ESP_LV_ADAPTER_ROTATE_0;
#endif
}

static esp_lv_adapter_tear_avoid_mode_t get_default_tear_mode(void)
{
#if CONFIG_EXAMPLE_LCD_INTERFACE_MIPI_DSI
    return ESP_LV_ADAPTER_TEAR_AVOID_MODE_DEFAULT_MIPI_DSI;
#elif CONFIG_EXAMPLE_LCD_INTERFACE_RGB
    return ESP_LV_ADAPTER_TEAR_AVOID_MODE_DEFAULT_RGB;
#else
    return ESP_LV_ADAPTER_TEAR_AVOID_MODE_DEFAULT;
#endif
}

static void action_button_event_cb(lv_event_t *e)
{
    if (lv_event_get_code(e) != LV_EVENT_CLICKED) {
        return;
    }

    if (example_uses_manual_sleep()) {
        ESP_LOGI(TAG, "Manual sleep requested from UI");
        esp_err_t ret = run_full_light_sleep_cycle();
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "Manual sleep flow failed: %s", esp_err_to_name(ret));
        }
        return;
    }

    s_activity_count++;
    (void)esp_lv_adapter_report_activity();
    update_status_label();
}

static void update_status_label(void)
{
    if (!s_status_label) {
        return;
    }

    if (esp_lv_adapter_lock(-1) != ESP_OK) {
        ESP_LOGW(TAG, "Failed to lock LVGL for label update");
        return;
    }

    if (example_uses_manual_sleep()) {
        lv_label_set_text_fmt(
            s_status_label,
            "%s\n"
            "Wake: timer in %d ms",
            example_get_sleep_hint_text(),
            CONFIG_EXAMPLE_LIGHT_SLEEP_WAKE_TIMER_MS);
    } else if (example_uses_pause_auto_sleep()) {
        lv_label_set_text_fmt(
            s_status_label,
            "Sleep: auto in %" PRIu32 " ms\n"
            "%s",
            example_get_auto_sleep_idle_timeout_ms(),
            example_get_wake_hint_text());
    } else {
        lv_label_set_text_fmt(
            s_status_label,
            "Sleep: auto in %" PRIu32 " ms\n"
            "%s",
            example_get_auto_sleep_idle_timeout_ms(),
            example_get_wake_hint_text());
    }

    esp_lv_adapter_unlock();
}

static esp_err_t validate_auto_sleep_platform_config(void)
{
    if (!example_uses_auto_sleep()) {
        return ESP_OK;
    }

#if CONFIG_SOC_DMA2D_SUPPORTED && !CONFIG_DMA2D_OPERATION_FUNC_IN_IRAM
    ESP_LOGE(TAG, "Auto sleep with DMA2D requires CONFIG_DMA2D_OPERATION_FUNC_IN_IRAM=y");
    return ESP_ERR_INVALID_STATE;
#endif

#if CONFIG_SOC_DMA2D_SUPPORTED && !CONFIG_PM_POWER_DOWN_PERIPHERAL_IN_LIGHT_SLEEP
    ESP_LOGW(TAG, "CONFIG_PM_POWER_DOWN_PERIPHERAL_IN_LIGHT_SLEEP is disabled; DMA2D sleep guard is inactive");
#endif

    return ESP_OK;
}

static esp_err_t configure_pm_for_auto_sleep(void)
{
#if CONFIG_PM_ENABLE
    esp_pm_config_t pm_config = {
        .max_freq_mhz = CONFIG_ESP_DEFAULT_CPU_FREQ_MHZ,
        .min_freq_mhz = CONFIG_ESP_DEFAULT_CPU_FREQ_MHZ,
        .light_sleep_enable = true,
    };
    ESP_RETURN_ON_ERROR(esp_pm_configure(&pm_config), TAG, "PM configure failed");
#endif

    return ESP_OK;
}

static esp_err_t example_unregister_touch_input(void)
{
#if HW_USE_TOUCH
    if (s_touch_indev) {
        ESP_RETURN_ON_ERROR(esp_lv_adapter_unregister_touch(s_touch_indev),
                            TAG, "Touch input unregister failed");
        s_touch_indev = NULL;
    }

    if (s_touch_handle) {
        ESP_RETURN_ON_ERROR(hw_touch_deinit(), TAG, "Touch hardware deinit failed");
        s_touch_handle = NULL;
    }
#endif

    return ESP_OK;
}

static esp_err_t example_register_touch_input(void)
{
#if HW_USE_TOUCH
    if (s_touch_indev) {
        return ESP_OK;
    }

    if (!s_touch_handle) {
        ESP_RETURN_ON_ERROR(hw_touch_init(&s_touch_handle, s_rotation), TAG, "Touch init failed");
    }

    esp_lv_adapter_touch_config_t touch_cfg = ESP_LV_ADAPTER_TOUCH_DEFAULT_CONFIG(s_disp, s_touch_handle);
    s_touch_indev = esp_lv_adapter_register_touch(&touch_cfg);
    ESP_RETURN_ON_FALSE(s_touch_indev != NULL, ESP_FAIL, TAG, "Touch input registration failed");
#endif

    return ESP_OK;
}

/* Optional production-grade robustness: best-effort restore of panel/touch/adapter
 * if a step of the sleep cycle fails. Not required to understand the basic flow. */
static esp_err_t recover_failed_light_sleep_cycle(bool panel_handles_valid, bool touch_registered)
{
    esp_err_t first_err = ESP_OK;

    if (!panel_handles_valid) {
        esp_err_t ret = hw_lcd_init(&s_panel_handle, &s_panel_io_handle, s_tear_mode, s_rotation);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "LCD restore failed: %s", esp_err_to_name(ret));
            first_err = ret;
        } else {
            panel_handles_valid = true;
        }
    }

    if (panel_handles_valid && !touch_registered) {
        esp_err_t ret = example_register_touch_input();
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "Touch restore failed: %s", esp_err_to_name(ret));
            if (first_err == ESP_OK) {
                first_err = ret;
            }
        }
    }

    if (panel_handles_valid) {
        esp_err_t ret = esp_lv_adapter_sleep_recover(s_disp, s_panel_handle, s_panel_io_handle);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "Adapter recover failed: %s", esp_err_to_name(ret));
            if (first_err == ESP_OK) {
                first_err = ret;
            }
        }
    }

    if (first_err != ESP_OK) {
        return first_err;
    }

    return ESP_OK;
}

static esp_err_t run_full_light_sleep_cycle(void)
{
    bool panel_handles_valid = true;
    bool touch_registered = true;
    esp_err_t ret = ESP_OK;

    /* Save the INT GPIO now -- s_touch_handle becomes NULL after unregister */
    gpio_num_t touch_int_gpio = example_touch_int_gpio();

    ESP_LOGI(TAG, "Entering full light sleep cycle");
    /* Adapter side: pause LVGL, flush pending frames, detach the panel, guard HW */
    ESP_RETURN_ON_ERROR(esp_lv_adapter_sleep_prepare(), TAG, "Sleep prepare failed");
    ret = example_unregister_touch_input();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Touch suspend failed: %s", esp_err_to_name(ret));
        goto recover;
    }
    touch_registered = false;

    ret = hw_lcd_deinit();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "LCD deinit failed: %s", esp_err_to_name(ret));
        goto recover;
    }
    panel_handles_valid = false;

    /*
     * App side: configure wakeup sources and enter light sleep.
     *
     * In USER mode the adapter does not manage sleep state -- the entire
     * cycle runs synchronously here.  esp_light_sleep_start() blocks until
     * any configured wakeup source fires, then returns normally.  No
     * request_wake call is needed; the adapter resumes via sleep_recover().
     *
     * Timer: guaranteed auto-wake after the configured interval.
     * Touch INT (if available): immediate wake on finger tap, same as PAUSE
     *   mode Step 1.  Unlike PAUSE mode there is no separate Step 2 here --
     *   the MCU returning from esp_light_sleep_start() is sufficient.
     */
    esp_sleep_disable_wakeup_source(ESP_SLEEP_WAKEUP_TIMER);
    ret = esp_sleep_enable_timer_wakeup((uint64_t)CONFIG_EXAMPLE_LIGHT_SLEEP_WAKE_TIMER_MS * 1000ULL);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Timer wakeup configure failed: %s", esp_err_to_name(ret));
        goto recover;
    }
    if (touch_int_gpio != GPIO_NUM_NC) {
        gpio_wakeup_enable(touch_int_gpio, GPIO_INTR_LOW_LEVEL);
        esp_sleep_enable_gpio_wakeup();
    }

    int64_t sleep_start_us = esp_timer_get_time();
    ret = esp_light_sleep_start();
    /* Disarm GPIO wakeup before re-registering touch so the ISR takes over */
    if (touch_int_gpio != GPIO_NUM_NC) {
        gpio_wakeup_disable(touch_int_gpio);
    }
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Light sleep start failed: %s", esp_err_to_name(ret));
        goto recover;
    }
    s_last_sleep_duration_us = (uint64_t)(esp_timer_get_time() - sleep_start_us);
    s_last_wake_causes = esp_sleep_get_wakeup_causes();
    s_user_sleep_cycles++;

    ret = hw_lcd_init(&s_panel_handle, &s_panel_io_handle, s_tear_mode, s_rotation);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "LCD reinit failed: %s", esp_err_to_name(ret));
        return ret;
    }
    panel_handles_valid = true;

    ret = example_register_touch_input();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Touch restore failed: %s", esp_err_to_name(ret));
        goto recover;
    }
    touch_registered = true;

    /* Adapter side: rebind the freshly re-inited panel, release HW guard, resume */
    ret = esp_lv_adapter_sleep_recover(s_disp, s_panel_handle, s_panel_io_handle);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Sleep recover failed: %s", esp_err_to_name(ret));
        esp_err_t recover_ret = recover_failed_light_sleep_cycle(true, touch_registered);
        if (recover_ret != ESP_OK) {
            ESP_LOGE(TAG, "Retry adapter recover failed: %s", esp_err_to_name(recover_ret));
        }
        return ret;
    }

    update_status_label();
    ESP_LOGI(TAG, "Light sleep cycle complete: wake causes=0x%" PRIx32 " duration=%" PRIu64 " ms",
             s_last_wake_causes, s_last_sleep_duration_us / 1000ULL);
    return ESP_OK;

recover: {
        esp_err_t recover_ret = recover_failed_light_sleep_cycle(panel_handles_valid, touch_registered);
        if (recover_ret != ESP_OK) {
            ESP_LOGE(TAG, "Failed to recover from light sleep error: %s", esp_err_to_name(recover_ret));
        }
        return ret;
    }
}

static esp_err_t example_panel_set_sleep(bool sleep)
{
    ESP_RETURN_ON_FALSE(s_panel_handle, ESP_ERR_INVALID_STATE, TAG, "Panel handle is NULL");

    esp_err_t ret = esp_lcd_panel_disp_sleep(s_panel_handle, sleep);
    if (ret == ESP_ERR_NOT_SUPPORTED) {
        ret = esp_lcd_panel_disp_on_off(s_panel_handle, !sleep);
    }

    ESP_RETURN_ON_ERROR(ret, TAG, "Panel %s failed", sleep ? "sleep entry" : "wake");
    return ESP_OK;
}

/* Fallback timer for pause mode (Step 2 via task context).
 * Fires inside light sleep via esp_timer; calls request_wake() to signal the
 * adapter task.  The MCU is already awake when this callback runs. */
static void pause_wake_timer_cb(void *arg)
{
    (void)arg;
    (void)esp_lv_adapter_request_wake();
}

static esp_err_t pause_wake_timer_ensure(void)
{
    if (s_pause_wake_timer) {
        return ESP_OK;
    }

    const esp_timer_create_args_t timer_args = {
        .callback = pause_wake_timer_cb,
        .name = "pause_wake",
    };
    return esp_timer_create(&timer_args, &s_pause_wake_timer);
}

/* Returns the touch interrupt GPIO if touch is initialized and has an INT pin,
 * GPIO_NUM_NC otherwise. */
static gpio_num_t example_touch_int_gpio(void)
{
#if HW_USE_TOUCH
    if (s_touch_handle && s_touch_handle->config.int_gpio_num != GPIO_NUM_NC) {
        return s_touch_handle->config.int_gpio_num;
    }
#endif
    return GPIO_NUM_NC;
}

/*
 * PAUSE mode sleep/wake callbacks.
 *
 * In PAUSE mode the adapter manages the LVGL side automatically:
 *   - on_enter_sleep fires first, then the adapter pauses LVGL and releases
 *     the ESP_PM_NO_LIGHT_SLEEP lock so tickless light sleep can begin.
 *   - on_exit_sleep fires after the adapter re-acquires the lock but before
 *     it resumes LVGL.
 *
 * Waking from light sleep via touch requires two independent steps:
 *
 *   Step 1 — MCU wakeup source (this callback's job):
 *     Configure gpio_wakeup_enable() so the touch INT pin can pull the MCU
 *     out of light sleep.  Without this the MCU stays asleep even when the
 *     touch chip asserts its interrupt.
 *
 *   Step 2 — Adapter resume (built-in, no code needed here):
 *     The touch driver's ISR calls esp_lv_adapter_request_wake_from_isr(),
 *     which signals the adapter task to exit SLEEPING state and resume LVGL.
 *
 * Both steps fire when the touch INT pin goes low:
 *   touch INT low -> [Step 1] GPIO wakeup pulls MCU out of light sleep
 *                 -> [Step 2] touch ISR calls request_wake_from_isr()
 *
 * A timer is also armed as a fallback so the panel auto-wakes periodically
 * even without user input.  The timer callback calls request_wake(), which
 * handles Step 2 from task context (the MCU is already awake at that point).
 */
static esp_err_t pause_mode_enter_sleep(void *user_ctx)
{
    (void)user_ctx;
    ESP_LOGI(TAG, "Auto sleep entering: pause mode");
    ESP_RETURN_ON_ERROR(example_panel_set_sleep(true), TAG, "Panel sleep failed");

    /* Step 1: arm the touch INT pin as GPIO wakeup source (MCU side) */
    gpio_num_t int_gpio = example_touch_int_gpio();
    if (int_gpio != GPIO_NUM_NC) {
        gpio_wakeup_enable(int_gpio, GPIO_INTR_LOW_LEVEL);
        esp_sleep_enable_gpio_wakeup();
    }

    /* Timer fallback: wake the adapter after the configured interval even
     * when there is no touch input (Step 2 handled in pause_wake_timer_cb) */
    ESP_RETURN_ON_ERROR(pause_wake_timer_ensure(), TAG, "Create pause wake timer failed");
    esp_timer_stop(s_pause_wake_timer); /* clear any previously armed shot */
    return esp_timer_start_once(s_pause_wake_timer,
                                (uint64_t)CONFIG_EXAMPLE_LIGHT_SLEEP_WAKE_TIMER_MS * 1000ULL);
}

static esp_err_t pause_mode_exit_sleep(void *user_ctx)
{
    (void)user_ctx;
    ESP_LOGI(TAG, "Auto sleep exiting: pause mode");
    /* Cancel the timer in case touch woke us before it fired */
    if (s_pause_wake_timer) {
        esp_timer_stop(s_pause_wake_timer);
    }
    /* Disarm the GPIO wakeup source; normal touch ISR operation resumes */
    gpio_num_t int_gpio = example_touch_int_gpio();
    if (int_gpio != GPIO_NUM_NC) {
        gpio_wakeup_disable(int_gpio);
    }
    return example_panel_set_sleep(false);
}

static esp_err_t user_mode_enter_sleep(void *user_ctx)
{
    (void)user_ctx;
    return run_full_light_sleep_cycle();
}

static esp_err_t configure_adapter_auto_sleep(esp_lv_adapter_config_t *adapter_config)
{
    ESP_RETURN_ON_FALSE(adapter_config, ESP_ERR_INVALID_ARG, TAG, "Adapter config is NULL");

    if (example_uses_manual_sleep()) {
        adapter_config->auto_sleep.enable = false;
        adapter_config->auto_sleep.mode = ESP_LV_ADAPTER_AUTO_SLEEP_MODE_DISABLED;
        adapter_config->auto_sleep.callbacks.on_enter_sleep = NULL;
        adapter_config->auto_sleep.callbacks.on_exit_sleep = NULL;
        adapter_config->auto_sleep.callbacks.user_ctx = NULL;
        return ESP_OK;
    }

    adapter_config->auto_sleep.enable = true;
    adapter_config->auto_sleep.idle_timeout_ms = example_get_auto_sleep_idle_timeout_ms();

    if (example_uses_pause_auto_sleep()) {
        /* PAUSE: adapter pauses LVGL; callbacks only blank/restore the panel */
        adapter_config->auto_sleep.mode = ESP_LV_ADAPTER_AUTO_SLEEP_MODE_PAUSE;
        adapter_config->auto_sleep.callbacks.on_enter_sleep = pause_mode_enter_sleep;
        adapter_config->auto_sleep.callbacks.on_exit_sleep = pause_mode_exit_sleep;
    } else {
        /* USER: on_enter_sleep runs the full light-sleep cycle synchronously */
        adapter_config->auto_sleep.mode = ESP_LV_ADAPTER_AUTO_SLEEP_MODE_USER;
        adapter_config->auto_sleep.callbacks.on_enter_sleep = user_mode_enter_sleep;
        adapter_config->auto_sleep.callbacks.on_exit_sleep = NULL;
    }

    adapter_config->auto_sleep.callbacks.user_ctx = NULL;
    return ESP_OK;
}

static void create_demo_ui(void)
{
    if (esp_lv_adapter_lock(-1) != ESP_OK) {
        ESP_LOGE(TAG, "Failed to lock LVGL for UI creation");
        return;
    }

    lv_obj_t *scr = lv_scr_act();
    lv_obj_set_style_bg_color(scr, lv_color_hex(0x101820), 0);
    lv_obj_set_style_bg_grad_color(scr, lv_color_hex(0x1C2A3A), 0);
    lv_obj_set_style_bg_grad_dir(scr, LV_GRAD_DIR_VER, 0);

    s_status_label = lv_label_create(scr);
    lv_obj_set_width(s_status_label, example_use_compact_ui() ? LV_PCT(92) : LV_PCT(88));
    lv_label_set_long_mode(s_status_label, LV_LABEL_LONG_WRAP);
    lv_obj_set_style_text_color(s_status_label, lv_color_hex(0xF5F7FA), 0);
    lv_obj_set_style_text_align(s_status_label, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_text_line_space(s_status_label, example_get_status_line_space(), 0);
    lv_obj_set_style_text_font(s_status_label, example_get_status_font(), 0);
    lv_obj_align(s_status_label, LV_ALIGN_TOP_MID, 0, example_get_status_top_offset());

    s_action_button = lv_btn_create(scr);
    lv_obj_set_size(s_action_button, example_use_compact_ui() ? LV_PCT(68) : LV_PCT(64), example_get_button_height());
    lv_obj_set_style_radius(s_action_button, example_get_button_radius(), 0);
    lv_obj_set_style_bg_color(s_action_button, lv_color_hex(0x56CFE1), 0);
    lv_obj_set_style_bg_grad_color(s_action_button, lv_color_hex(0x72EFDD), 0);
    lv_obj_set_style_bg_grad_dir(s_action_button, LV_GRAD_DIR_HOR, 0);
    lv_obj_set_style_text_color(s_action_button, lv_color_hex(0x08131D), 0);
    lv_obj_align(s_action_button, LV_ALIGN_BOTTOM_MID, 0, example_get_button_bottom_offset());
    lv_obj_add_event_cb(s_action_button, action_button_event_cb, LV_EVENT_CLICKED, NULL);

    lv_obj_t *btn_label = lv_label_create(s_action_button);
    lv_label_set_text(btn_label, example_get_action_button_text());
    lv_obj_set_style_text_font(btn_label, example_get_button_font(), 0);
    lv_obj_set_style_text_color(btn_label, lv_color_hex(0x08131D), 0);
    lv_obj_center(btn_label);

    lv_label_set_text(s_status_label, "Preparing sleep demo...");
    esp_lv_adapter_unlock();

    update_status_label();
    bind_encoder_to_ui();
}

void app_main(void)
{
    ESP_LOGI(TAG, "LVGL Sleep Demo");
    ESP_LOGI(TAG, "LCD resolution %dx%d", HW_LCD_H_RES, HW_LCD_V_RES);
    ESP_LOGI(TAG, "Sleep mode: %s", example_get_auto_sleep_mode_name());
    ESP_ERROR_CHECK(validate_auto_sleep_platform_config());

    // Get configuration
    s_rotation = get_configured_rotation();
    s_tear_mode = get_default_tear_mode();

    // Initialize LCD hardware
    ESP_ERROR_CHECK(hw_lcd_init(&s_panel_handle, &s_panel_io_handle, s_tear_mode, s_rotation));
    ESP_ERROR_CHECK(configure_pm_for_auto_sleep());

    // Initialize adapter
    esp_lv_adapter_config_t adapter_config = ESP_LV_ADAPTER_DEFAULT_CONFIG();
    ESP_ERROR_CHECK(configure_adapter_auto_sleep(&adapter_config));
    ESP_ERROR_CHECK(esp_lv_adapter_init(&adapter_config));

    // Register display
#if CONFIG_EXAMPLE_LCD_INTERFACE_MIPI_DSI
    esp_lv_adapter_display_config_t display_cfg = ESP_LV_ADAPTER_DISPLAY_MIPI_DEFAULT_CONFIG(
                                                      s_panel_handle, s_panel_io_handle,
                                                      HW_LCD_H_RES, HW_LCD_V_RES, s_rotation);
#elif CONFIG_EXAMPLE_LCD_INTERFACE_RGB
    esp_lv_adapter_display_config_t display_cfg = ESP_LV_ADAPTER_DISPLAY_RGB_DEFAULT_CONFIG(
                                                      s_panel_handle, s_panel_io_handle,
                                                      HW_LCD_H_RES, HW_LCD_V_RES, s_rotation);
#elif CONFIG_EXAMPLE_LCD_INTERFACE_SPI_WITHOUT_PSRAM
    esp_lv_adapter_display_config_t display_cfg = ESP_LV_ADAPTER_DISPLAY_SPI_WITHOUT_PSRAM_DEFAULT_CONFIG(
                                                      s_panel_handle, s_panel_io_handle,
                                                      HW_LCD_H_RES, HW_LCD_V_RES, s_rotation);
#else
    esp_lv_adapter_display_config_t display_cfg = ESP_LV_ADAPTER_DISPLAY_SPI_WITH_PSRAM_DEFAULT_CONFIG(
                                                      s_panel_handle, s_panel_io_handle,
                                                      HW_LCD_H_RES, HW_LCD_V_RES, s_rotation);
#endif

    s_disp = esp_lv_adapter_register_display(&display_cfg);
    if (s_disp == NULL) {
        ESP_LOGE(TAG, "Display registration failed");
        return;
    }

    // Register input device (optional)
#if HW_USE_TOUCH
    ESP_ERROR_CHECK(example_register_touch_input());
#elif HW_USE_ENCODER && CONFIG_ESP_LVGL_ADAPTER_ENABLE_KNOB
    esp_lv_adapter_encoder_config_t encoder_cfg = {
        .disp = s_disp,
        .encoder_a_b = hw_knob_get_config(),
        .encoder_enter = hw_knob_get_button(),
    };
    s_encoder = esp_lv_adapter_register_encoder(&encoder_cfg);
    if (s_encoder == NULL) {
        ESP_LOGE(TAG, "Encoder registration failed");
    }
#endif

    // Start adapter
    ESP_ERROR_CHECK(esp_lv_adapter_start());

    // Create UI
    create_demo_ui();

    if (example_uses_auto_sleep()) {
        ESP_ERROR_CHECK(esp_lv_adapter_report_activity());
    }
}

static void bind_encoder_to_ui(void)
{
    if (!s_encoder || !s_action_button) {
        return;
    }

    if (esp_lv_adapter_lock(-1) != ESP_OK) {
        ESP_LOGE(TAG, "Failed to lock LVGL for encoder binding");
        return;
    }

    lv_group_t *group = lv_group_get_default();
    if (!group) {
        group = lv_group_create();
        lv_group_set_default(group);
    }

    if (!group) {
        ESP_LOGE(TAG, "Failed to create default input group");
        esp_lv_adapter_unlock();
        return;
    }

    lv_group_remove_all_objs(group);
    lv_group_add_obj(group, s_action_button);
    lv_group_focus_obj(s_action_button);
    lv_indev_set_group(s_encoder, group);

    esp_lv_adapter_unlock();
}
