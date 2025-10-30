/*
 * SPDX-FileCopyrightText: 2024-2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdio.h>
#include <string.h>

#include "driver/gpio.h"
#include "esp_log.h"
#include "esp_pm.h"
#include "esp_sleep.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/timers.h"
#include "nvs_flash.h"

#include "ble_hci.h"
#include "bthome_v2.h"
#include "iot_button.h"
#include "iot_knob.h"

#define GPIO_OUTPUT_PIN_SEL  (1ULL << CONFIG_EXAMPLE_POWER_CTRL_IO_NUM)

static const char *TAG = "bthome_dimmer";

typedef enum {
    TASK_EVENT_NONE = 0,
    TASK_EVENT_BTN,
    TASK_EVENT_KNOB,
} dimmer_task_event_t;

typedef struct {
    knob_event_t event;
    int value;
} knob_report_t;

typedef struct {
    knob_handle_t knob;
    button_handle_t btn;
    bthome_handle_t bthome;
    TaskHandle_t task_handle;
    TimerHandle_t timer_handle;
    knob_report_t knob_report;
    button_event_t button_event;
} dimmer_t;

static const uint8_t encrypt_key[] = {0x23, 0x1d, 0x39, 0xc1, 0xd7, 0xcc, 0x1a, 0xb1, 0xae, 0xe2, 0x24, 0xcd, 0x09, 0x6d, 0xb9, 0x32};
static const uint8_t local_mac[] = { 0x54, 0x48, 0xE6, 0x8F, 0x80, 0xA5};
static const uint8_t peer_mac[] = { 0x54, 0x48, 0xE6, 0x8F, 0x80, 0xA6};
static dimmer_t *s_dimmer;

static void knob_event_cb(void *arg, void *data)
{
    knob_event_t evt = iot_knob_get_event(arg);
    if (s_dimmer != NULL) {
        s_dimmer->knob_report.event = evt;
        s_dimmer->knob_report.value = iot_knob_get_count_value((knob_handle_t)arg);
        BaseType_t xHigherPriorityTaskWoken = pdFALSE;
        xTaskNotifyFromISR(s_dimmer->task_handle, TASK_EVENT_KNOB, eSetValueWithOverwrite, &xHigherPriorityTaskWoken);
    }
}

static void button_event_cb(void *arg, void *data)
{
    esp_sleep_wakeup_cause_t cause = esp_sleep_get_wakeup_cause();
    if (cause != ESP_SLEEP_WAKEUP_UNDEFINED) {
        ESP_LOGI(TAG, "Wake up from light sleep, reason %d", cause);
    }
    if (s_dimmer != NULL) {
        s_dimmer->button_event = (button_event_t)data;
        BaseType_t xHigherPriorityTaskWoken = pdFALSE;
        xTaskNotifyFromISR(s_dimmer->task_handle, TASK_EVENT_BTN, eSetValueWithOverwrite, &xHigherPriorityTaskWoken);
    }
}

void timer_callback(TimerHandle_t xTimer)
{
    ble_hci_set_adv_enable(false);
}

static void dimmer_task(void *arg)
{
    uint8_t advertisement_data[31] = {0};
    uint8_t payload_length = 0;
    uint8_t payload_data[31];
    ble_hci_addr_t local_mac_addr = {0};
    memcpy((uint8_t *)local_mac_addr, local_mac, BLE_HCI_ADDR_LEN);
    ble_hci_set_random_address(local_mac_addr);

    ble_hci_adv_param_t adv_param = {
        .adv_int_min = 0x50,
        .adv_int_max = 0x50,
        .adv_type = ADV_TYPE_NONCONN_IND,
        .own_addr_type = BLE_ADDR_TYPE_RANDOM,
        .peer_addr_type = BLE_ADDR_TYPE_PUBLIC,
        .channel_map = ADV_CHNL_ALL,
        .adv_filter_policy = ADV_FILTER_ALLOW_SCAN_ANY_CON_ANY,
    };
    memcpy(adv_param.peer_addr, peer_mac, BLE_HCI_ADDR_LEN);
    ble_hci_set_adv_param(&adv_param);

    uint8_t dim_evt[2] = {0};
    uint8_t btn_evt_id = 0x00;
    uint8_t adv_len = 0;
    uint8_t name[] = {0x44, 0x49, 0x59};
    bthome_device_info_t info = {
        .bit.bthome_version = 2,
        .bit.encryption_flag = true,
        .bit.trigger_based_flag = 0,
    };
    uint32_t ulNotificationValue;

    while (1) {
        if (xTaskNotifyWait(0x00, 0xFFFFFFFF, &ulNotificationValue, pdMS_TO_TICKS(3000)) == pdTRUE) {
            payload_length = 0;
            if (ulNotificationValue == TASK_EVENT_BTN) {
                if (s_dimmer->button_event == BUTTON_SINGLE_CLICK) {
                    btn_evt_id = 1;
                } else {
                    btn_evt_id = 0;
                }
                dim_evt[0] = 0;
                dim_evt[1] = 0;
            } else if (ulNotificationValue == TASK_EVENT_KNOB) {
                btn_evt_id = 0;
                if (s_dimmer->knob_report.value > 0) {
                    dim_evt[0] = 1; //left
                    dim_evt[1] = s_dimmer->knob_report.value;
                } else {
                    dim_evt[0] = 2; //right
                    dim_evt[1] = -s_dimmer->knob_report.value;
                }
            }
            payload_length = bthome_payload_adv_add_evt_data(payload_data, payload_length, BTHOME_EVENT_ID_BUTTON, &btn_evt_id, 1);
            payload_length = bthome_payload_adv_add_evt_data(payload_data, payload_length, BTHOME_EVENT_ID_DIMMER, dim_evt, 2);
            adv_len = bthome_make_adv_data(s_dimmer->bthome, advertisement_data, name, sizeof(name), info, payload_data, payload_length);
            ESP_LOGI(TAG, "adv_len %d", adv_len);
            ESP_LOG_BUFFER_HEX_LEVEL("advertisement_data", advertisement_data, adv_len, ESP_LOG_WARN);
            ble_hci_set_adv_data(adv_len, advertisement_data);
            ble_hci_set_adv_enable(true);
            if (xTimerIsTimerActive(s_dimmer->timer_handle) == false) {
                xTimerStart(s_dimmer->timer_handle, 0);
            } else {
                xTimerReset(s_dimmer->timer_handle, 0);
            }
        } else {
            continue;
        }
    }
    vTaskDelay(pdMS_TO_TICKS(10));
    gpio_hold_dis(CONFIG_EXAMPLE_POWER_CTRL_IO_NUM);
    gpio_set_level(CONFIG_EXAMPLE_POWER_CTRL_IO_NUM, 0);
    if (s_dimmer != NULL) {
        free(s_dimmer);
        s_dimmer = NULL;
    }
    vTaskDelete(NULL);
}

static void power_ctrl_io_init(void)
{
    gpio_set_level(CONFIG_EXAMPLE_POWER_CTRL_IO_NUM, 1);
    //zero-initialize the config structure.
    gpio_config_t io_conf = {};
    //disable interrupt
    io_conf.intr_type = GPIO_INTR_DISABLE;
    //set as output mode
    io_conf.mode = GPIO_MODE_OUTPUT;
    //bit mask of the pins that you want to set,e.g.GPIO18/19
    io_conf.pin_bit_mask = GPIO_OUTPUT_PIN_SEL;
    //disable pull-down mode
    io_conf.pull_down_en = 0;
    //disable pull-up mode
    io_conf.pull_up_en = 0;
    //configure GPIO with the given settings
    gpio_config(&io_conf);
    gpio_set_level(CONFIG_EXAMPLE_POWER_CTRL_IO_NUM, 1);
    gpio_hold_en(CONFIG_EXAMPLE_POWER_CTRL_IO_NUM);
}

static void knob_init(void)
{
    knob_config_t cfg = {
        .default_direction = 0,
        .gpio_encoder_a = CONFIG_EXAMPLE_GPIO_KNOB_A,
        .gpio_encoder_b = CONFIG_EXAMPLE_GPIO_KNOB_B,
#if CONFIG_PM_ENABLE
        .enable_power_save = true,
#endif
    };
    s_dimmer->knob = iot_knob_create(&cfg);
    iot_knob_register_cb(s_dimmer->knob, KNOB_LEFT, knob_event_cb, NULL);
    iot_knob_register_cb(s_dimmer->knob, KNOB_RIGHT, knob_event_cb, NULL);
}

static esp_err_t button_init(void)
{
    button_config_t btn_cfg = {
        .type = BUTTON_TYPE_GPIO,
        .gpio_button_config = {
            .gpio_num = CONFIG_EXAMPLE_BUTTON_IO_NUM,
            .active_level = CONFIG_EXAMPLE_BUTTON_ACTIVE_LEVEL,
#if CONFIG_GPIO_BUTTON_SUPPORT_POWER_SAVE
            .enable_power_save = true,
#endif
        },
    };
    s_dimmer->btn = iot_button_create(&btn_cfg);
    assert(s_dimmer->btn);
    esp_err_t err = iot_button_register_cb(s_dimmer->btn, BUTTON_PRESS_DOWN, button_event_cb, (void *)BUTTON_SINGLE_CLICK);
    return err;
}

static void settings_store(bthome_handle_t handle, const char *key, const uint8_t *data, uint8_t len)
{
    nvs_handle_t nvs_handle;
    esp_err_t err = nvs_open("storage", NVS_READWRITE, &nvs_handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Error opening NVS handle: %s", esp_err_to_name(err));
        return;
    }

    err = nvs_set_blob(nvs_handle, key, data, len);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to write blob to NVS: %s", esp_err_to_name(err));
    } else {
        err = nvs_commit(nvs_handle);
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "Failed to commit to NVS: %s", esp_err_to_name(err));
        }
    }

    nvs_close(nvs_handle);
}

static void settings_load(bthome_handle_t handle, const char *key, uint8_t *data, uint8_t len)
{
    nvs_handle_t nvs_handle;
    esp_err_t err = nvs_open("storage", NVS_READWRITE, &nvs_handle);
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "Error opening NVS handle: %s", esp_err_to_name(err));
        memset(data, 0, len);
        return;
    }

    size_t required_size = len;
    err = nvs_get_blob(nvs_handle, key, data, &required_size);
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "Failed to read blob from NVS: %s", esp_err_to_name(err));
        memset(data, 0, len);
    }

    nvs_close(nvs_handle);
}

void app_main(void)
{
    // Initialize NVS
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

#if CONFIG_PM_ENABLE
    // Configure dynamic frequency scaling:
    // maximum and minimum frequencies are set in sdkconfig,
    // automatic light sleep is enabled if tickless idle support is enabled.
    esp_pm_config_t pm_config = {
        .max_freq_mhz = CONFIG_EXAMPLE_MAX_CPU_FREQ_MHZ,
        .min_freq_mhz = CONFIG_EXAMPLE_MIN_CPU_FREQ_MHZ,
#if CONFIG_FREERTOS_USE_TICKLESS_IDLE
        .light_sleep_enable = true
#endif
    };
    ESP_ERROR_CHECK(esp_pm_configure(&pm_config));
#endif // CONFIG_PM_ENABLE

    s_dimmer = calloc(1, sizeof(dimmer_t));
    if (s_dimmer == NULL) {
        ESP_LOGE(TAG, "Failed to allocate memory for dimmer");
        return;
    }
    power_ctrl_io_init();
    ble_hci_init();
    bthome_create(&s_dimmer->bthome);

    bthome_callbacks_t callbacks = {
        .store = settings_store,
        .load = settings_load,
    };
    bthome_register_callbacks(s_dimmer->bthome, &callbacks);
    bthome_set_encrypt_key(s_dimmer->bthome, encrypt_key);
    bthome_set_local_mac_addr(s_dimmer->bthome, (uint8_t *)local_mac);
    bthome_load_params(s_dimmer->bthome);

    esp_err_t res = xTaskCreate(dimmer_task, "dimmer task", 4096, NULL, 10, &s_dimmer->task_handle);
    if (res != pdPASS) {
        ESP_LOGE(TAG, "create dimmer task failed");
        return;
    }
    s_dimmer->timer_handle = xTimerCreate("cutsom adv timer", pdMS_TO_TICKS(300), pdFALSE, (void *)0, timer_callback);

    knob_init();
    button_init();

    xTaskNotify(s_dimmer->task_handle, TASK_EVENT_BTN, eSetValueWithOverwrite);
}
