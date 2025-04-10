/* SPDX-FileCopyrightText: 2022-2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "esp_log.h"
#include "esp_err.h"
#include "esp_check.h"
#include "driver/gpio.h"
#include "button_gpio.h"
#include "esp_sleep.h"
#include "button_interface.h"
#include "iot_button.h"

static const char *TAG = "gpio_button";

typedef struct {
    button_driver_t base;          /**< button driver */
    int32_t gpio_num;              /**< num of gpio */
    uint8_t active_level;          /**< gpio level when press down */
    bool enable_power_save;        /**< enable power save */
} button_gpio_obj;

static esp_err_t button_gpio_del(button_driver_t *button_driver)
{
    button_gpio_obj *gpio_btn = __containerof(button_driver, button_gpio_obj, base);
    esp_err_t ret = gpio_reset_pin(gpio_btn->gpio_num);
    free(gpio_btn);
    return ret;
}

static uint8_t button_gpio_get_key_level(button_driver_t *button_driver)
{
    button_gpio_obj *gpio_btn = __containerof(button_driver, button_gpio_obj, base);
    int level = gpio_get_level(gpio_btn->gpio_num);
    return level == gpio_btn->active_level ? 1 : 0;
}

static esp_err_t button_gpio_enable_gpio_wakeup(uint32_t gpio_num, uint8_t active_level, bool enable)
{
    esp_err_t ret;
    if (enable) {
        gpio_intr_enable(gpio_num);
        ret = gpio_wakeup_enable(gpio_num, active_level == 0 ? GPIO_INTR_LOW_LEVEL : GPIO_INTR_HIGH_LEVEL);
    } else {
        gpio_intr_disable(gpio_num);
        ret = gpio_wakeup_disable(gpio_num);
    }
    return ret;
}

static esp_err_t button_gpio_set_intr(int gpio_num, gpio_int_type_t intr_type, gpio_isr_t isr_handler)
{
    static bool isr_service_installed = false;
    gpio_set_intr_type(gpio_num, intr_type);
    if (!isr_service_installed) {
        gpio_install_isr_service(ESP_INTR_FLAG_IRAM);
        isr_service_installed = true;
    }
    gpio_isr_handler_add(gpio_num, isr_handler, (void *)gpio_num);
    return ESP_OK;
}

static void button_power_save_isr_handler(void* arg)
{
    /*!< resume the button */
    iot_button_resume();
    /*!< disable gpio wakeup not need active level*/
    button_gpio_enable_gpio_wakeup((uint32_t)arg, 0, false);
}

static esp_err_t button_enter_power_save(button_driver_t *button_driver)
{
    button_gpio_obj *gpio_btn = __containerof(button_driver, button_gpio_obj, base);
    return button_gpio_enable_gpio_wakeup(gpio_btn->gpio_num, gpio_btn->active_level, true);
}

esp_err_t iot_button_new_gpio_device(const button_config_t *button_config, const button_gpio_config_t *gpio_cfg, button_handle_t *ret_button)
{
    button_gpio_obj *gpio_btn = NULL;
    esp_err_t ret = ESP_OK;
    ESP_GOTO_ON_FALSE(button_config && gpio_cfg && ret_button, ESP_ERR_INVALID_ARG, err, TAG, "Invalid argument");
    ESP_GOTO_ON_FALSE(GPIO_IS_VALID_GPIO(gpio_cfg->gpio_num), ESP_ERR_INVALID_ARG, err, TAG, "GPIO number error");

    gpio_btn = (button_gpio_obj *)calloc(1, sizeof(button_gpio_obj));
    ESP_GOTO_ON_FALSE(gpio_btn, ESP_ERR_NO_MEM, err, TAG, "No memory for gpio button");
    gpio_btn->gpio_num = gpio_cfg->gpio_num;
    gpio_btn->active_level = gpio_cfg->active_level;
    gpio_btn->enable_power_save = gpio_cfg->enable_power_save;

    gpio_config_t gpio_conf = {0};
    gpio_conf.intr_type = GPIO_INTR_DISABLE;
    gpio_conf.mode = GPIO_MODE_INPUT;
    gpio_conf.pin_bit_mask = (1ULL << gpio_cfg->gpio_num);
    if (!gpio_cfg->disable_pull) {
        if (gpio_cfg->active_level) {
            gpio_conf.pull_down_en = GPIO_PULLDOWN_ENABLE;
            gpio_conf.pull_up_en = GPIO_PULLUP_DISABLE;
        } else {
            gpio_conf.pull_down_en = GPIO_PULLDOWN_DISABLE;
            gpio_conf.pull_up_en = GPIO_PULLUP_ENABLE;
        }
    }
    gpio_config(&gpio_conf);

    if (gpio_cfg->enable_power_save) {
#if CONFIG_PM_POWER_DOWN_PERIPHERAL_IN_LIGHT_SLEEP
        if (!esp_sleep_is_valid_wakeup_gpio(gpio_cfg->gpio_num)) {
            ESP_LOGE(TAG, "GPIO %ld is not a valid wakeup source under CONFIG_GPIO_BUTTON_SUPPORT_POWER_SAVE", gpio_cfg->gpio_num);
            return ESP_FAIL;
        }
        gpio_hold_en(gpio_cfg->gpio_num);
#endif
        /* Enable wake up from GPIO */
        esp_err_t ret = gpio_wakeup_enable(gpio_cfg->gpio_num, gpio_cfg->active_level == 0 ? GPIO_INTR_LOW_LEVEL : GPIO_INTR_HIGH_LEVEL);
        ESP_GOTO_ON_FALSE(ret == ESP_OK, ESP_ERR_INVALID_STATE, err, TAG, "Enable gpio wakeup failed");
#if CONFIG_PM_POWER_DOWN_PERIPHERAL_IN_LIGHT_SLEEP
#if SOC_PM_SUPPORT_EXT1_WAKEUP
        ret = esp_sleep_enable_ext1_wakeup_io((1ULL << gpio_cfg->gpio_num), gpio_cfg->active_level == 0 ? ESP_EXT1_WAKEUP_ANY_LOW : ESP_EXT1_WAKEUP_ANY_HIGH);
#else
        /*!< Not support etc: esp32c2, esp32c3. Target must support ext1 wakeup */
        ret = ESP_FAIL;
        ESP_GOTO_ON_FALSE(ret == ESP_OK, ESP_FAIL, err, TAG, "Target must support ext1 wakeup");
#endif
#else
        ret = esp_sleep_enable_gpio_wakeup();
#endif
        ESP_GOTO_ON_FALSE(ret == ESP_OK, ESP_FAIL, err, TAG, "Configure gpio as wakeup source failed");

        ret = button_gpio_set_intr(gpio_btn->gpio_num, gpio_cfg->active_level == 0 ? GPIO_INTR_LOW_LEVEL : GPIO_INTR_HIGH_LEVEL, button_power_save_isr_handler);
        ESP_GOTO_ON_FALSE(ret == ESP_OK, ESP_FAIL, err, TAG, "Set gpio interrupt failed");

        gpio_btn->base.enable_power_save = true;
        gpio_btn->base.enter_power_save = button_enter_power_save;
    }

    gpio_btn->base.get_key_level = button_gpio_get_key_level;
    gpio_btn->base.del = button_gpio_del;

    ret = iot_button_create(button_config, &gpio_btn->base, ret_button);
    ESP_GOTO_ON_FALSE(ret == ESP_OK, ESP_FAIL, err, TAG, "Create button failed");

    return ESP_OK;
err:
    if (gpio_btn) {
        free(gpio_btn);
    }
    return ret;
}
