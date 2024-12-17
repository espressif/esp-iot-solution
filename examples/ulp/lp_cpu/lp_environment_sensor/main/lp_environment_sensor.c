/*
 * SPDX-FileCopyrightText: 2023-2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include <stdio.h>
#include <math.h>
#include <time.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "freertos/event_groups.h"
#include "esp_log.h"
#include "esp_event.h"
#include "esp_sleep.h"
#include "esp_system.h"
#include "nvs_flash.h"
#include "lp_core_main.h"
#include "ulp_lp_core.h"
#include "lp_core_i2c.h"
#include "driver/rtc_io.h"
#include "ulp_lp_core_lp_timer_shared.h"
#ifdef CONFIG_ULP_UART_PRINT
#include "lp_core_uart.h"
#endif
#ifdef CONFIG_REPORT_THS_DATA
#include <esp_rmaker_core.h>
#include <esp_rmaker_standard_params.h>
#include <esp_rmaker_standard_devices.h>
#include <esp_rmaker_standard_types.h>
#include <esp_rmaker_user_mapping.h>
#include <esp_rmaker_common_events.h>
#include <app_wifi.h>
#ifdef CONFIG_UPDATE_WEATHER_DATA
#include "app_http.h"
#endif
#endif
#include "driver/gpio.h"
#include "app_epaper.h"

static const char *TAG = "LP_Environment_Sensor";

extern const uint8_t lp_core_main_bin_start[] asm("_binary_lp_core_main_bin_start");
extern const uint8_t lp_core_main_bin_end[]   asm("_binary_lp_core_main_bin_end");

/* E-paper power control pin, set to 1 to power off the screen, set to 0 to power on the screen. */
#define LDO_CHIP_EN                 GPIO_NUM_2
#define EPAPER_PWR_CTRL             GPIO_NUM_8
#define EPAPER_BUS_HOST             SPI2_HOST
#define EPAPER_REFRESH_BIT          BIT(0)
#define WIFI_CONNECT_SUCCESS_BIT    BIT(1)
#define WIFI_CONNECT_FAIL_BIT       BIT(2)
#define RMAKER_DATA_REPORT_BIT      BIT(3)
#define WEATHER_DATA_UPDATE_BIT     BIT(4)
#define WIFI_UNAVAILABLE            BIT(5)

static EventGroupHandle_t esp_ths_event_group = NULL;

#ifdef CONFIG_REPORT_THS_DATA
static esp_rmaker_device_t *temp_sensor_device;
#endif

static epaper_handle_t s_epaper = NULL;

static RTC_SLOW_ATTR http_weather_data_t weather_data = {
    // District ID for weather information retrieval (needs to be queried on Baidu Developer Platform)
    .district_id = {0},
    .city_name = {0},
    .district_name = {0},
    .text = {0},
    .wind_class = {0},
    .wind_dir = {0},
    .uptime = {0},
    .week = {0},
    .temp_high = 0,
    .temp_low = 0
};

typedef struct {
    float temp;
    float hum;
    bool enable_http;
} esp_ths_data_type_t;

static void lp_core_init(void)
{
    esp_err_t ret = ESP_OK;
    ulp_lp_core_cfg_t cfg = {
        .wakeup_source = ULP_LP_CORE_WAKEUP_SOURCE_LP_TIMER,
        .lp_timer_sleep_duration_us = CONFIG_LP_CPU_WAKEUP_TIME_SECOND * 1000000,
    };

    ret = ulp_lp_core_load_binary(lp_core_main_bin_start, (lp_core_main_bin_end - lp_core_main_bin_start));
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "LP Core load failed");
        abort();
    }

    ret = ulp_lp_core_run(&cfg);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "LP Core run failed");
        abort();
    }
}

static void lp_i2c_init(void)
{
    esp_err_t ret = ESP_OK;
    /* Initialize LP I2C with default configuration */
    const lp_core_i2c_cfg_t i2c_cfg = {
        .i2c_pin_cfg.sda_io_num = GPIO_NUM_6,
        .i2c_pin_cfg.scl_io_num = GPIO_NUM_7,
        .i2c_pin_cfg.sda_pullup_en = true,
        .i2c_pin_cfg.scl_pullup_en = true,
        .i2c_timing_cfg.clk_speed_hz = 400000,
        .i2c_src_clk = LP_I2C_SCLK_LP_FAST,
    };
    ret = lp_core_i2c_master_init(LP_I2C_NUM_0, &i2c_cfg);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "LP I2C init failed");
        abort();
    }
}

#ifdef CONFIG_ULP_UART_PRINT
static void lp_uart_init(void)
{
    lp_core_uart_cfg_t cfg = {
        .uart_pin_cfg.tx_io_num = GPIO_NUM_5,
        .uart_pin_cfg.rx_io_num = GPIO_NUM_4,
        .uart_pin_cfg.rts_io_num = GPIO_NUM_2,
        .uart_pin_cfg.cts_io_num = GPIO_NUM_3,
        .uart_proto_cfg.baud_rate = 115200,
        .uart_proto_cfg.data_bits = UART_DATA_8_BITS,
        .uart_proto_cfg.parity = UART_PARITY_DISABLE,
        .uart_proto_cfg.stop_bits = UART_STOP_BITS_1,
        .uart_proto_cfg.flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
        .uart_proto_cfg.rx_flow_ctrl_thresh = 0,
        .lp_uart_source_clk = LP_UART_SCLK_DEFAULT,
    };
    ESP_ERROR_CHECK(lp_core_uart_init(&cfg));
}
#endif

#ifdef CONFIG_REPORT_THS_DATA
static void rmaker_data_report_done_event_handler(void* arg, esp_event_base_t event_base,
                                                  int32_t event_id, void* event_data)
{
    if (event_base == RMAKER_COMMON_EVENT) {
        if (event_id == RMAKER_MQTT_EVENT_PUBLISHED) {
            xEventGroupSetBits(esp_ths_event_group, RMAKER_DATA_REPORT_BIT);
        }
    }
}

static void rmaker_mqtt_disconnected_event_handler(void* arg, esp_event_base_t event_base,
                                                   int32_t event_id, void* event_data)
{
    if (event_base == RMAKER_COMMON_EVENT) {
        if (event_id == RMAKER_MQTT_EVENT_DISCONNECTED) {
            xEventGroupSetBits(esp_ths_event_group, WIFI_UNAVAILABLE);
        }
    }
}

static void rmaker_task(void *pvParameters)
{
    esp_ths_data_type_t *data = (esp_ths_data_type_t *)pvParameters;
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);
    if (data->hum == 0) {
        app_first_time_wifi_init();
    } else {
        app_second_time_wifi_init();
    }

    esp_event_handler_register(RMAKER_COMMON_EVENT, RMAKER_MQTT_EVENT_PUBLISHED,
                               &rmaker_data_report_done_event_handler, NULL);
    esp_event_handler_register(RMAKER_COMMON_EVENT, RMAKER_MQTT_EVENT_DISCONNECTED,
                               &rmaker_mqtt_disconnected_event_handler, NULL);
    esp_rmaker_config_t rainmaker_cfg = {
        .enable_time_sync = false,
    };
    esp_rmaker_node_t *node = esp_rmaker_node_init(&rainmaker_cfg, "ESP RainMaker Device", "ESP-THS");

    temp_sensor_device = esp_rmaker_temp_sensor_device_create("ESP-THS", NULL, roundf((data->temp) * 10.0) / 10.0);
    esp_rmaker_param_t *temp_param = esp_rmaker_param_create("温度(Temperature)", NULL, esp_rmaker_float(roundf((data->temp) * 10.0) / 10.0), PROP_FLAG_READ);
    esp_rmaker_param_t *rh_param = esp_rmaker_param_create("湿度(Humidity)", NULL, esp_rmaker_float(roundf((data->hum) * 10.0) / 10.0), PROP_FLAG_READ);
    esp_rmaker_device_add_param(temp_sensor_device, temp_param);
    esp_rmaker_device_add_param(temp_sensor_device, rh_param);
    esp_rmaker_device_assign_primary_param(temp_sensor_device, temp_param);
    esp_rmaker_node_add_device(node, temp_sensor_device);

    esp_rmaker_start();

    if (data->hum == 0) {
        if (app_first_time_wifi_start(POP_TYPE_RANDOM) == ESP_OK) {
            xEventGroupSetBits(esp_ths_event_group, WIFI_CONNECT_SUCCESS_BIT);
        } else {
            xEventGroupSetBits(esp_ths_event_group, WIFI_CONNECT_FAIL_BIT);
        }
    } else {
        if (app_second_time_wifi_start() == ESP_OK) {
            xEventGroupSetBits(esp_ths_event_group, WIFI_CONNECT_SUCCESS_BIT);
        } else {
            xEventGroupSetBits(esp_ths_event_group, WIFI_CONNECT_FAIL_BIT);
        }
    }
    while (1) {
        vTaskDelay(10 / portTICK_PERIOD_MS);
    }
    vTaskDelete(NULL);
}
#endif

#if CONFIG_UPDATE_WEATHER_DATA
static void http_task(void *pvParameters)
{
    while (1) {
        EventBits_t bits = xEventGroupWaitBits(esp_ths_event_group,
                                               WIFI_CONNECT_SUCCESS_BIT | WIFI_CONNECT_FAIL_BIT,
                                               pdFALSE,
                                               pdFALSE,
                                               portMAX_DELAY);
        if (bits & WIFI_CONNECT_SUCCESS_BIT) {
            if (get_http_weather_data_with_url(&weather_data) == ESP_OK) {
                ESP_LOGI(TAG, "Weather data get done.");
                xEventGroupSetBits(esp_ths_event_group, WEATHER_DATA_UPDATE_BIT);
            } else {
                ESP_LOGE(TAG, "Weather data get failed.");
                xEventGroupSetBits(esp_ths_event_group, WIFI_UNAVAILABLE);
            }
        } else {
            ESP_LOGE(TAG, "Wi-Fi connection failed.");
        }
        vTaskDelete(NULL);
    }
}
#endif

static void epaper_task(void *pvParameters)
{
    uint8_t refresh_times = 1;
    esp_ths_data_type_t *data = (esp_ths_data_type_t *)pvParameters;

    gpio_config_t epaper_pwr_io_conf = {};
    epaper_pwr_io_conf.pin_bit_mask = (1ULL << EPAPER_PWR_CTRL);
    epaper_pwr_io_conf.mode = GPIO_MODE_OUTPUT;
    epaper_pwr_io_conf.pull_up_en = true;
    gpio_config(&epaper_pwr_io_conf);
    gpio_set_level(EPAPER_PWR_CTRL, 0);

    s_epaper = epaper_create(EPAPER_BUS_HOST);
    if (s_epaper == NULL) {
        ESP_LOGE(TAG, "epaper create failed");
    }

    epaper_display_data_t epaper_display_data = {
        .city = weather_data.city_name,
        .district = weather_data.district_name,
        .temp = data->temp,
        .hum = data->hum,
        .text = weather_data.text,
        .week = weather_data.week,
        .max_temp = weather_data.temp_high,
        .min_temp = weather_data.temp_low,
        .wind_class = weather_data.wind_class,
        .wind_dir = weather_data.wind_dir,
        .uptime = weather_data.uptime,
    };

#ifdef CONFIG_REGION_CHINA
    epaper_display_data.city = CONFIG_CHINA_CITY_NAME;
    epaper_display_data.district = CONFIG_CHINA_DISTRICT_NAME;
#endif

    while (1) {
        if (data->enable_http == true) {
            if (CONFIG_UPDATE_WEATHER_DATA) {
                EventBits_t bits = xEventGroupWaitBits(esp_ths_event_group,
                                                       WEATHER_DATA_UPDATE_BIT | WIFI_UNAVAILABLE | WIFI_CONNECT_FAIL_BIT,
                                                       pdFALSE,
                                                       pdFALSE,
                                                       portMAX_DELAY);
                if (bits & WEATHER_DATA_UPDATE_BIT) {
                    ESP_LOGI(TAG, "http get data done.");
                    ESP_LOGI(TAG, "week: %s", weather_data.week);
                    ESP_LOGI(TAG, "Highest temp: %d, lowest temp: %d", weather_data.temp_high, weather_data.temp_low);
                    ESP_LOGI(TAG, "weather: %s", weather_data.text);
                    ESP_LOGI(TAG, "wind class: %s, wind dir: %s", weather_data.wind_class, weather_data.wind_dir);
                    ESP_LOGI(TAG, "uptime: %s", weather_data.uptime);
                    epaper_display_data.max_temp = weather_data.temp_high;
                    epaper_display_data.min_temp = weather_data.temp_low;
                    epaper_display(s_epaper, epaper_display_data);
                    gpio_set_level(EPAPER_PWR_CTRL, 1);
                    xEventGroupSetBits(esp_ths_event_group, EPAPER_REFRESH_BIT);
                } else {
                    ESP_LOGE(TAG, "http get data failed.");
                    epaper_display_data.text = "No Wifi";
                    snprintf(weather_data.text, sizeof(weather_data.text), "%s", "No Wifi");
                    epaper_display(s_epaper, epaper_display_data);
                    gpio_set_level(EPAPER_PWR_CTRL, 1);
                    xEventGroupSetBits(esp_ths_event_group, EPAPER_REFRESH_BIT);
                }
            } else {
                if (refresh_times) {
                    epaper_display(s_epaper, epaper_display_data);
                    xEventGroupSetBits(esp_ths_event_group, EPAPER_REFRESH_BIT);
                    gpio_set_level(EPAPER_PWR_CTRL, 1);
                    refresh_times--;
                }
            }
        } else {
            EventBits_t bits = xEventGroupWaitBits(esp_ths_event_group,
                                                   WIFI_CONNECT_SUCCESS_BIT | WIFI_CONNECT_FAIL_BIT,
                                                   pdFALSE,
                                                   pdFALSE,
                                                   portMAX_DELAY);
            if (bits & WIFI_CONNECT_FAIL_BIT) {
                epaper_display_data.text = "No Wifi";
                snprintf(weather_data.text, sizeof(weather_data.text), "%s", "No Wifi");
            }
            if (refresh_times) {
                epaper_display(s_epaper, epaper_display_data);
                xEventGroupSetBits(esp_ths_event_group, EPAPER_REFRESH_BIT);
                gpio_set_level(EPAPER_PWR_CTRL, 1);
                refresh_times--;
            }
        }
        vTaskDelete(NULL);
    }
}

void app_main(void)
{
    /*!< Config charge IC enable IO */
    gpio_set_level(LDO_CHIP_EN, 1);
    gpio_config_t io_conf = {
        .pin_bit_mask = (1ULL << LDO_CHIP_EN),
        .mode = GPIO_MODE_OUTPUT,
    };
    gpio_config(&io_conf);
    gpio_set_level(LDO_CHIP_EN, 1);
    gpio_hold_en(LDO_CHIP_EN);

    /* To refresh the e-paper screen, it takes at least 1250ms. */
    esp_ths_event_group = xEventGroupCreate();
    if (esp_ths_event_group == NULL) {
        ESP_LOGE(TAG, "Failed to create esp-ths event group.");
    }

    esp_sleep_wakeup_cause_t cause = esp_sleep_get_wakeup_cause();
    if (cause != ESP_SLEEP_WAKEUP_ULP) {
        const esp_ths_data_type_t sensor_data = {
            .temp = 0,
            .hum = 0,
            .enable_http = true,
        };
#ifdef CONFIG_REPORT_THS_DATA
        xTaskCreate(rmaker_task, "rmaker_task", 1024 * 8, (void *)&sensor_data, 5, NULL);
#endif

#if CONFIG_UPDATE_WEATHER_DATA
        xTaskCreate(http_task, "http_task", 1024 * 8, NULL, 10, NULL);
#endif
        xTaskCreate(epaper_task, "epaper_task", 1024 * 8, (void *)&sensor_data, 8, NULL);
        /* Wait for the e-paper refresh to be completed. */
        EventBits_t bits = xEventGroupWaitBits(esp_ths_event_group,
                                               EPAPER_REFRESH_BIT,
                                               pdTRUE,
                                               pdFALSE,
                                               portMAX_DELAY);
        if (bits & EPAPER_REFRESH_BIT) {
            ESP_LOGI(TAG, "E-paper refresh done.");
        }
        epaper_delete(&s_epaper);
#ifdef CONFIG_REPORT_THS_DATA
        /* Wait for successful reporting of temperature and humidity data to
         * Rainmaker before proceeding to the next step.
         */
        bits = xEventGroupWaitBits(esp_ths_event_group,
                                   RMAKER_DATA_REPORT_BIT | WIFI_UNAVAILABLE | WIFI_CONNECT_FAIL_BIT,
                                   pdFALSE,
                                   pdFALSE,
                                   portMAX_DELAY);
        if (bits & RMAKER_DATA_REPORT_BIT) {
            ESP_LOGI(TAG, "Report esp-ths data done.");
        } else if (bits & WIFI_UNAVAILABLE) {
            ESP_LOGE(TAG, "WiFi is unavailable.");
        } else {
            ESP_LOGE(TAG, "Wi-Fi connection failed.");
        }
        vTaskDelay(10 / portTICK_PERIOD_MS);
#endif
        /* Initialize LP_I2C */
        lp_i2c_init();
#ifdef CONFIG_ULP_UART_PRINT
        /* Initialize LP_UART */
        lp_uart_init();
#endif
        /* Load LP Core binary and start the coprocessor */
        lp_core_init();
    } else if (cause == ESP_SLEEP_WAKEUP_ULP) {
        printf("wake up by ULP.\n");
        float *temp = (float*)(&ulp_temp);
        float *hum = (float *)(&ulp_hum);
        bool weather_data_report = false;
#if CONFIG_UPDATE_WEATHER_DATA
        weather_data_report = (ulp_weather_data_update == 1) ? true : false;
#endif
        const esp_ths_data_type_t sensor_data = {
            .temp = *temp,
            .hum = *hum,
            .enable_http = weather_data_report
        };
        ESP_LOGI(TAG, "temperature: %f, humidity: %f", sensor_data.temp, sensor_data.hum);
#if CONFIG_UPDATE_WEATHER_DATA
        if (sensor_data.enable_http == true) {
            xTaskCreate(http_task, "http_task", 1024 * 8, NULL, 10, NULL);
        }
#endif

#ifdef CONFIG_REPORT_THS_DATA
        xTaskCreate(rmaker_task, "rmaker_task", 1024 * 8, (void *)&sensor_data, 5, NULL);
#endif
        xTaskCreate(epaper_task, "epaper_task", 1024 * 8, (void *)&sensor_data, 8, NULL);
#ifdef CONFIG_REPORT_THS_DATA
        esp_rmaker_param_update_and_report(
            esp_rmaker_device_get_param_by_type(temp_sensor_device, ESP_RMAKER_PARAM_TEMPERATURE),
            esp_rmaker_float(roundf((*temp) * 10.0) / 10.0));
#endif
        /* Wait for the e-paper refresh to be completed. */
        EventBits_t bits = xEventGroupWaitBits(esp_ths_event_group,
                                               EPAPER_REFRESH_BIT,
                                               pdTRUE,
                                               pdFALSE,
                                               portMAX_DELAY);
        if (bits & EPAPER_REFRESH_BIT) {
            ESP_LOGI(TAG, "E-paper refresh done.");
        }
        epaper_delete(&s_epaper);
#ifdef CONFIG_REPORT_THS_DATA
        /* Wait for successful reporting of temperature and humidity data to
         * Rainmaker before proceeding to the next step.
         */
        bits = xEventGroupWaitBits(esp_ths_event_group,
                                   RMAKER_DATA_REPORT_BIT | WIFI_UNAVAILABLE | WIFI_CONNECT_FAIL_BIT,
                                   pdFALSE,
                                   pdFALSE,
                                   portMAX_DELAY);
        if (bits & RMAKER_DATA_REPORT_BIT) {
            ESP_LOGI(TAG, "Report esp-ths data done.");
        } else if (bits & WIFI_UNAVAILABLE) {
            ESP_LOGE(TAG, "WiFi is unavailable.");
        } else {
            ESP_LOGE(TAG, "Wi-Fi connection failed.");
        }
        vTaskDelay(10 / portTICK_PERIOD_MS);
#endif
#if CONFIG_UPDATE_WEATHER_DATA
        if (ulp_weather_data_update == 1) {
            ulp_weather_data_update = 0;
            ulp_measure_times = 0;
        }
#endif
        ulp_sensor_data_report = 0;
        printf("wake up by ULP.\n");
    }
    /* Setup wakeup triggers */
    ESP_ERROR_CHECK(esp_sleep_enable_ulp_wakeup());
    /* Enter Deep Sleep */
    printf("Enter deep sleep.\n");
    esp_deep_sleep_start();
}
