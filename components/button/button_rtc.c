/* SPDX-FileCopyrightText: 2022-2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "esp_log.h"
#include "esp_err.h"
#include "esp_check.h"
#include "driver/gpio.h"
#include "driver/rtc_io.h"
#include "button_rtc.h"
#include "esp_sleep.h"
#include "button_interface.h"
#include "iot_button.h"
#include "soc/soc_caps.h"

#if SOC_RTCIO_INPUT_OUTPUT_SUPPORTED

static const char *TAG = "rtc_button";

typedef struct {
    button_driver_t base;
    int32_t gpio_num;
    uint8_t active_level;
    bool enable_power_save;
} button_rtc_obj;

static esp_err_t button_rtc_enable_wakeup(uint32_t gpio_num, uint8_t active_level, bool enable);

static esp_err_t button_rtc_set_wakeup_sources(uint32_t gpio_num, uint8_t active_level, bool enable)
{
    esp_err_t ret;
    if (enable) {
#if CONFIG_PM_POWER_DOWN_PERIPHERAL_IN_LIGHT_SLEEP
#if SOC_PM_SUPPORT_EXT1_WAKEUP
        ret = esp_sleep_enable_ext1_wakeup_io((1ULL << gpio_num),
                                              active_level == 0 ? ESP_EXT1_WAKEUP_ANY_LOW : ESP_EXT1_WAKEUP_ANY_HIGH);
        ESP_RETURN_ON_FALSE(ret == ESP_OK, ret, TAG, "Enable ext1 wakeup failed");
#else
        return ESP_FAIL;
#endif
#endif
        ret = rtc_gpio_wakeup_enable(gpio_num, active_level == 0 ? GPIO_INTR_LOW_LEVEL : GPIO_INTR_HIGH_LEVEL);
    } else {
#if CONFIG_PM_POWER_DOWN_PERIPHERAL_IN_LIGHT_SLEEP
#if SOC_PM_SUPPORT_EXT1_WAKEUP
        esp_sleep_disable_ext1_wakeup_io(1ULL << gpio_num);
#endif
#endif
        ret = rtc_gpio_wakeup_disable(gpio_num);
    }
    return ret;
}

static esp_err_t button_rtc_del(button_driver_t *button_driver)
{
    button_rtc_obj *rtc_btn = __containerof(button_driver, button_rtc_obj, base);
    if (rtc_btn->enable_power_save) {
        button_rtc_enable_wakeup(rtc_btn->gpio_num, rtc_btn->active_level, false);
        gpio_isr_handler_remove(rtc_btn->gpio_num);
#if CONFIG_PM_POWER_DOWN_PERIPHERAL_IN_LIGHT_SLEEP
        rtc_gpio_hold_dis(rtc_btn->gpio_num);
#endif
    }
    esp_err_t ret = rtc_gpio_deinit(rtc_btn->gpio_num);
    free(rtc_btn);
    return ret;
}

static uint8_t button_rtc_get_key_level(button_driver_t *button_driver)
{
    button_rtc_obj *rtc_btn = __containerof(button_driver, button_rtc_obj, base);
    int level = rtc_gpio_get_level(rtc_btn->gpio_num);
    return level == rtc_btn->active_level ? 1 : 0;
}

static esp_err_t button_rtc_enable_wakeup(uint32_t gpio_num, uint8_t active_level, bool enable)
{
    esp_err_t ret = button_rtc_set_wakeup_sources(gpio_num, active_level, enable);
    if (ret != ESP_OK) {
        return ret;
    }
    if (enable) {
        gpio_intr_enable(gpio_num);
    } else {
        gpio_intr_disable(gpio_num);
    }
    return ESP_OK;
}

static int32_t button_rtc_get_gpio_num(button_driver_t *button_driver)
{
    button_rtc_obj *rtc_btn = __containerof(button_driver, button_rtc_obj, base);
    return rtc_btn->gpio_num;
}

static esp_err_t button_rtc_exit_power_save(button_driver_t *button_driver)
{
    button_rtc_obj *rtc_btn = __containerof(button_driver, button_rtc_obj, base);
    return button_rtc_enable_wakeup(rtc_btn->gpio_num, rtc_btn->active_level, false);
}

static esp_err_t gpio_isr_service_ensure_installed(void)
{
    esp_err_t ret = gpio_install_isr_service(ESP_INTR_FLAG_IRAM);
    if (ret == ESP_OK || ret == ESP_ERR_INVALID_STATE) {
        return ESP_OK;
    }
    return ret;
}

static esp_err_t button_rtc_set_intr(int gpio_num, gpio_int_type_t intr_type, gpio_isr_t isr_handler)
{
    esp_err_t ret = gpio_set_intr_type(gpio_num, intr_type);
    ESP_RETURN_ON_FALSE(ret == ESP_OK, ret, TAG, "Set gpio interrupt type failed");
    ret = gpio_isr_service_ensure_installed();
    ESP_RETURN_ON_FALSE(ret == ESP_OK, ret, TAG, "Install gpio interrupt service failed");
    ret = gpio_isr_handler_add(gpio_num, isr_handler, (void *)gpio_num);
    ESP_RETURN_ON_FALSE(ret == ESP_OK, ret, TAG, "Add gpio interrupt handler failed");
    return ESP_OK;
}

static void IRAM_ATTR button_power_save_isr_handler(void *arg)
{
    iot_button_power_save_wakeup_isr((uint32_t)arg);
}

static esp_err_t button_enter_power_save(button_driver_t *button_driver)
{
    button_rtc_obj *rtc_btn = __containerof(button_driver, button_rtc_obj, base);
    return button_rtc_enable_wakeup(rtc_btn->gpio_num, rtc_btn->active_level, true);
}

esp_err_t iot_button_new_rtc_device(const button_config_t *button_config, const button_rtc_config_t *rtc_cfg, button_handle_t *ret_button)
{
    button_rtc_obj *rtc_btn = NULL;
    esp_err_t ret = ESP_OK;
    ESP_GOTO_ON_FALSE(button_config && rtc_cfg && ret_button, ESP_ERR_INVALID_ARG, err, TAG, "Invalid argument");
    *ret_button = NULL;
    ESP_GOTO_ON_FALSE(rtc_gpio_is_valid_gpio(rtc_cfg->gpio_num), ESP_ERR_INVALID_ARG, err, TAG, "RTC GPIO number error");

    rtc_btn = (button_rtc_obj *)calloc(1, sizeof(button_rtc_obj));
    ESP_GOTO_ON_FALSE(rtc_btn, ESP_ERR_NO_MEM, err, TAG, "No memory for rtc button");
    rtc_btn->gpio_num = rtc_cfg->gpio_num;
    rtc_btn->active_level = rtc_cfg->active_level;
    rtc_btn->enable_power_save = rtc_cfg->enable_power_save;

    ret = rtc_gpio_init(rtc_cfg->gpio_num);
    ESP_GOTO_ON_FALSE(ret == ESP_OK, ret, err, TAG, "RTC GPIO init failed");
    ret = rtc_gpio_set_direction(rtc_cfg->gpio_num, RTC_GPIO_MODE_INPUT_ONLY);
    ESP_GOTO_ON_FALSE(ret == ESP_OK, ret, err, TAG, "RTC GPIO set direction failed");

    if (!rtc_cfg->disable_pull) {
        if (rtc_cfg->active_level) {
            ESP_GOTO_ON_FALSE(rtc_gpio_pulldown_en(rtc_cfg->gpio_num) == ESP_OK, ESP_FAIL, err, TAG, "RTC pulldown enable failed");
            rtc_gpio_pullup_dis(rtc_cfg->gpio_num);
        } else {
            ESP_GOTO_ON_FALSE(rtc_gpio_pullup_en(rtc_cfg->gpio_num) == ESP_OK, ESP_FAIL, err, TAG, "RTC pullup enable failed");
            rtc_gpio_pulldown_dis(rtc_cfg->gpio_num);
        }
    }

    if (rtc_cfg->enable_power_save) {
#if CONFIG_PM_POWER_DOWN_PERIPHERAL_IN_LIGHT_SLEEP
        rtc_gpio_hold_en(rtc_cfg->gpio_num);
#endif
        ret = button_rtc_set_wakeup_sources(rtc_cfg->gpio_num, rtc_cfg->active_level, true);
        ESP_GOTO_ON_FALSE(ret == ESP_OK, ESP_FAIL, err, TAG, "Enable rtc gpio wakeup failed");
#if !CONFIG_PM_POWER_DOWN_PERIPHERAL_IN_LIGHT_SLEEP
        ret = esp_sleep_enable_gpio_wakeup();
        ESP_GOTO_ON_FALSE(ret == ESP_OK, ESP_FAIL, err, TAG, "Configure rtc gpio as wakeup source failed");
#endif

        rtc_btn->base.enable_power_save = true;
        rtc_btn->base.enter_power_save = button_enter_power_save;
        rtc_btn->base.exit_power_save = button_rtc_exit_power_save;
        rtc_btn->base.get_gpio_num = button_rtc_get_gpio_num;
    }

    rtc_btn->base.get_key_level = button_rtc_get_key_level;
    rtc_btn->base.del = button_rtc_del;

    ret = iot_button_create(button_config, &rtc_btn->base, ret_button);
    ESP_GOTO_ON_FALSE(ret == ESP_OK, ESP_FAIL, err, TAG, "Create button failed");

    if (rtc_cfg->enable_power_save) {
        ret = button_rtc_set_intr(rtc_btn->gpio_num,
                                  rtc_cfg->active_level == 0 ? GPIO_INTR_LOW_LEVEL : GPIO_INTR_HIGH_LEVEL,
                                  button_power_save_isr_handler);
        ESP_GOTO_ON_FALSE(ret == ESP_OK, ESP_FAIL, err, TAG, "Set rtc gpio interrupt failed");
    }
    return ESP_OK;
err:
    if (ret_button && *ret_button) {
        iot_button_delete(*ret_button);
        *ret_button = NULL;
    } else if (rtc_btn) {
        if (rtc_cfg && rtc_cfg->enable_power_save) {
            button_rtc_set_wakeup_sources(rtc_btn->gpio_num, rtc_btn->active_level, false);
#if CONFIG_PM_POWER_DOWN_PERIPHERAL_IN_LIGHT_SLEEP
            rtc_gpio_hold_dis(rtc_btn->gpio_num);
#endif
        }
        rtc_gpio_deinit(rtc_btn->gpio_num);
        free(rtc_btn);
    }
    return ret;
}

#else

esp_err_t iot_button_new_rtc_device(const button_config_t *button_config, const button_rtc_config_t *rtc_cfg, button_handle_t *ret_button)
{
    (void)button_config;
    (void)rtc_cfg;
    (void)ret_button;
    return ESP_ERR_NOT_SUPPORTED;
}

#endif
