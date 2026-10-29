/*
 * SPDX-FileCopyrightText: 2024-2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "esp_log.h"
#include "esp_sleep.h"
#include "esp_check.h"
#include "driver/gpio.h"
#include "driver/rtc_io.h"
#include "knob_hal.h"
#include "soc/soc_caps.h"

#if SOC_RTCIO_INPUT_OUTPUT_SUPPORTED

static const char *TAG = "knob rtc";

esp_err_t knob_rtc_init(uint32_t gpio_num)
{
    ESP_RETURN_ON_FALSE(rtc_gpio_is_valid_gpio(gpio_num), ESP_ERR_INVALID_ARG, TAG, "RTC GPIO number error");
    ESP_RETURN_ON_ERROR(rtc_gpio_init(gpio_num), TAG, "RTC GPIO init failed");
    ESP_RETURN_ON_ERROR(rtc_gpio_set_direction(gpio_num, RTC_GPIO_MODE_INPUT_ONLY), TAG, "RTC GPIO set direction failed");
    ESP_RETURN_ON_ERROR(rtc_gpio_pullup_en(gpio_num), TAG, "RTC pullup enable failed");
    ESP_RETURN_ON_ERROR(rtc_gpio_pulldown_dis(gpio_num), TAG, "RTC pulldown disable failed");
    return ESP_OK;
}

esp_err_t knob_rtc_deinit(uint32_t gpio_num)
{
#if CONFIG_PM_POWER_DOWN_PERIPHERAL_IN_LIGHT_SLEEP
    rtc_gpio_hold_dis(gpio_num);
#endif
    return rtc_gpio_deinit(gpio_num);
}

uint8_t knob_rtc_get_key_level(void *gpio_num)
{
    return (uint8_t)rtc_gpio_get_level((uint32_t)gpio_num);
}

static esp_err_t gpio_isr_service_ensure_installed(void)
{
    esp_err_t ret = gpio_install_isr_service(ESP_INTR_FLAG_IRAM);
    if (ret == ESP_OK || ret == ESP_ERR_INVALID_STATE) {
        return ESP_OK;
    }
    return ret;
}

esp_err_t knob_rtc_init_intr(uint32_t gpio_num, gpio_int_type_t intr_type, gpio_isr_t isr_handler, void *args)
{
    esp_err_t ret;
    ESP_RETURN_ON_ERROR(gpio_set_intr_type(gpio_num, intr_type), TAG, "Set gpio interrupt type failed");
    ret = gpio_isr_service_ensure_installed();
    ESP_RETURN_ON_FALSE(ret == ESP_OK, ret, TAG, "Install gpio interrupt service failed");
    ESP_RETURN_ON_ERROR(gpio_isr_handler_add(gpio_num, isr_handler, args), TAG, "Add gpio interrupt handler failed");
    return ESP_OK;
}

esp_err_t knob_rtc_set_intr(uint32_t gpio_num, gpio_int_type_t intr_type)
{
    return gpio_set_intr_type(gpio_num, intr_type);
}

esp_err_t knob_rtc_intr_control(uint32_t gpio_num, bool enable)
{
    if (enable) {
        return gpio_intr_enable(gpio_num);
    }
    return gpio_intr_disable(gpio_num);
}

esp_err_t knob_rtc_wake_up_control(uint32_t gpio_num, uint8_t wake_up_level, bool enable)
{
    esp_err_t ret;
    if (enable) {
#if CONFIG_PM_POWER_DOWN_PERIPHERAL_IN_LIGHT_SLEEP
#if SOC_PM_SUPPORT_EXT1_WAKEUP
        ret = esp_sleep_enable_ext1_wakeup_io((1ULL << gpio_num), wake_up_level == 0 ? ESP_EXT1_WAKEUP_ANY_LOW : ESP_EXT1_WAKEUP_ANY_HIGH);
        ESP_RETURN_ON_FALSE(ret == ESP_OK, ESP_FAIL, TAG, "Enable ext1 wakeup failed");
#else
        ret = ESP_FAIL;
        ESP_RETURN_ON_FALSE(ret == ESP_OK, ESP_FAIL, TAG, "Target must support ext1 wakeup");
#endif
#endif
        ret = rtc_gpio_wakeup_enable(gpio_num, wake_up_level == 0 ? GPIO_INTR_LOW_LEVEL : GPIO_INTR_HIGH_LEVEL);
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

esp_err_t knob_rtc_wake_up_init(uint32_t gpio_num, uint8_t wake_up_level)
{
#if CONFIG_PM_POWER_DOWN_PERIPHERAL_IN_LIGHT_SLEEP
    ESP_RETURN_ON_FALSE(rtc_gpio_is_valid_gpio(gpio_num), ESP_ERR_INVALID_ARG, TAG, "RTC GPIO number error");
    rtc_gpio_hold_en(gpio_num);
#endif
#if CONFIG_PM_POWER_DOWN_PERIPHERAL_IN_LIGHT_SLEEP
    ESP_RETURN_ON_ERROR(rtc_gpio_wakeup_enable(gpio_num, wake_up_level == 0 ? GPIO_INTR_LOW_LEVEL : GPIO_INTR_HIGH_LEVEL),
                        TAG, "Enable rtc gpio wakeup failed");
#if SOC_PM_SUPPORT_EXT1_WAKEUP
    ESP_RETURN_ON_ERROR(esp_sleep_enable_ext1_wakeup_io((1ULL << gpio_num),
                                                        wake_up_level == 0 ? ESP_EXT1_WAKEUP_ANY_LOW : ESP_EXT1_WAKEUP_ANY_HIGH),
                        TAG, "Enable ext1 wakeup failed");
#else
    return ESP_FAIL;
#endif
#else
    ESP_RETURN_ON_ERROR(rtc_gpio_wakeup_enable(gpio_num, wake_up_level == 0 ? GPIO_INTR_LOW_LEVEL : GPIO_INTR_HIGH_LEVEL),
                        TAG, "Enable rtc gpio wakeup failed");
    ESP_RETURN_ON_ERROR(esp_sleep_enable_gpio_wakeup(), TAG, "esp sleep enable gpio wakeup failed");
#endif
    return ESP_OK;
}

const knob_hal_t knob_rtc_hal = {
    .init = knob_rtc_init,
    .deinit = knob_rtc_deinit,
    .get_key_level = knob_rtc_get_key_level,
    .init_intr = knob_rtc_init_intr,
    .set_intr = knob_rtc_set_intr,
    .intr_control = knob_rtc_intr_control,
    .wake_up_control = knob_rtc_wake_up_control,
    .wake_up_init = knob_rtc_wake_up_init,
};

#endif
