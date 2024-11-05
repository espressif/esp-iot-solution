/*
 * SPDX-FileCopyrightText: 2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "esp_log.h"
#include "esp_sleep.h"
#include "esp_check.h"
#include "driver/gpio.h"
#include "knob_gpio.h"

static const char *TAG = "knob gpio";

esp_err_t knob_gpio_init(uint32_t gpio_num)
{
    gpio_config_t gpio_cfg = {
        .pin_bit_mask = (1ULL << gpio_num),
        .mode = GPIO_MODE_INPUT,
        .intr_type = GPIO_INTR_DISABLE,
        .pull_up_en = 1,
    };
    esp_err_t ret = gpio_config(&gpio_cfg);

    return ret;
}

esp_err_t knob_gpio_deinit(uint32_t gpio_num)
{
    return gpio_reset_pin(gpio_num);
}

uint8_t knob_gpio_get_key_level(void *gpio_num)
{
    return (uint8_t)gpio_get_level((uint32_t)gpio_num);
}

esp_err_t knob_gpio_init_intr(uint32_t gpio_num, gpio_int_type_t intr_type, gpio_isr_t isr_handler, void *args)
{
    static bool isr_service_installed = false;
    gpio_set_intr_type(gpio_num, intr_type);
    if (!isr_service_installed) {
        gpio_install_isr_service(ESP_INTR_FLAG_IRAM);
        isr_service_installed = true;
    }
    gpio_isr_handler_add(gpio_num, isr_handler, args);
    return ESP_OK;
}

esp_err_t knob_gpio_set_intr(uint32_t gpio_num, gpio_int_type_t intr_type)
{
    return gpio_set_intr_type(gpio_num, intr_type);
}

esp_err_t knob_gpio_intr_control(uint32_t gpio_num, bool enable)
{
    if (enable) {
        gpio_intr_enable(gpio_num);
    } else {
        gpio_intr_disable(gpio_num);
    }
    return ESP_OK;
}

esp_err_t knob_gpio_wake_up_control(uint32_t gpio_num, uint8_t wake_up_level, bool enable)
{
    esp_err_t ret;
    if (enable) {
#if CONFIG_PM_POWER_DOWN_PERIPHERAL_IN_LIGHT_SLEEP
#if SOC_PM_SUPPORT_EXT1_WAKEUP
        ret = esp_sleep_enable_ext1_wakeup_io((1ULL << gpio_num), wake_up_level == 0 ? ESP_EXT1_WAKEUP_ANY_LOW : ESP_EXT1_WAKEUP_ANY_HIGH);
#else
        /*!< Not support etc: esp32c2, esp32c3. Target must support ext1 wakeup */
        ret = ESP_FAIL;
        ESP_RETURN_ON_FALSE(ret == ESP_OK, ESP_FAIL, TAG, "Target must support ext1 wakeup");
#endif
#endif
        /* Enable wake up from GPIO */
        ret = gpio_wakeup_enable(gpio_num, wake_up_level == 0 ? GPIO_INTR_LOW_LEVEL : GPIO_INTR_HIGH_LEVEL);
    } else {
#if CONFIG_PM_POWER_DOWN_PERIPHERAL_IN_LIGHT_SLEEP
#if SOC_PM_SUPPORT_EXT1_WAKEUP
        ret = esp_sleep_disable_ext1_wakeup_io(1ULL << gpio_num);
#endif
#endif
        ret = gpio_wakeup_disable(gpio_num);
    }
    return ret;
}

esp_err_t knob_gpio_wake_up_init(uint32_t gpio_num, uint8_t wake_up_level)
{
#if CONFIG_PM_POWER_DOWN_PERIPHERAL_IN_LIGHT_SLEEP
    if (!esp_sleep_is_valid_wakeup_gpio(gpio_num)) {
        ESP_LOGE(TAG, "GPIO %ld is not a valid wakeup source under CONFIG_PM_POWER_DOWN_PERIPHERAL_IN_LIGHT_SLEEP",
                 gpio_num);
        return ESP_FAIL;
    }
    gpio_hold_en(gpio_num);
#endif
    /* Enable wake up from GPIO */
    esp_err_t ret = gpio_wakeup_enable(gpio_num, wake_up_level == 0 ? GPIO_INTR_LOW_LEVEL : GPIO_INTR_HIGH_LEVEL);
    ESP_RETURN_ON_FALSE(ret == ESP_OK, ESP_FAIL, TAG, "Enable gpio wakeup failed");

#if CONFIG_PM_POWER_DOWN_PERIPHERAL_IN_LIGHT_SLEEP
#if SOC_PM_SUPPORT_EXT1_WAKEUP
    ret = esp_sleep_enable_ext1_wakeup_io((1ULL << gpio_num), wake_up_level == 0 ? ESP_EXT1_WAKEUP_ANY_LOW : ESP_EXT1_WAKEUP_ANY_HIGH);
#else
    /*!< Not support etc: esp32c2, esp32c3. Target must support ext1 wakeup */
    ret = ESP_FAIL;
    ESP_RETURN_ON_FALSE(ret == ESP_OK, ESP_FAIL, TAG, "Target must support ext1 wakeup");
#endif
#else
    ret = esp_sleep_enable_gpio_wakeup();
    ESP_RETURN_ON_FALSE(ret == ESP_OK, ESP_FAIL, TAG, "esp sleep enable gpio wakeup failed");
#endif
    return ESP_OK;
}
