/* Lowpower EVB Example

   For other examples please check:
   https://github.com/espressif/esp-iot-solution/tree/master/examples

   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
 */

#include "string.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "esp_system.h"
#include "esp_log.h"
#include "esp_sleep.h"
#include "iot_wifi_conn.h"
#include "iot_tcp.h"
#include "lowpower_framework.h"
#include "lowpower_evb_callback.h"
#include "lowpower_evb_adc.h"
#include "lowpower_evb_epaper.h"
#include "lowpower_evb_sensor.h"
#include "lowpower_evb_status_led.h"
#include "lowpower_evb_wifi.h"
#include "lowpower_evb_ulp_opt.h"

static const char *TAG = "lowpower_evb_callback";

/* Gpio wake up */
#define GPIO_WAKE_UP_IO             (34)
#define GPIO_WAKE_UP_LEVEL          (0)

/* Fixed wifi config */
#ifdef CONFIG_USE_FIXED_ROUTER
#define USE_FIXED_ROUTER
#define FIXED_ROUTER_SSID     CONFIG_FIXED_ROUTER_SSID
#define FIXED_ROUTER_PASSWORD CONFIG_FIXED_ROUTER_PASSWORD
#endif

/* Use ESP-TOUCH to config wifi */
#ifdef CONFIG_USE_ESP_TOUCH_CONFIG
#define USE_ESP_TOUCH_CONFIG
#endif

/* Server connect */
#define SERVER_IP   CONFIG_SERVER_IP
#define SERVER_PORT CONFIG_SERVER_PORT

/* upload data size */
#define UPLOAD_DATA_SIZE CONFIG_UPLOAD_DATA_SIZE

/* Server sensor number and interval */
#define READ_SENSOR_VALUE_NUM CONFIG_READ_SENSOR_VALUE_NUM
#define READ_SENSOR_INTERVAL  CONFIG_READ_SENSOR_INTERVAL

/* Timer wake up time */
#define WAKEUP_TIME_US CONFIG_WAKEUP_TIME_US

/* enbale epaper */
#ifdef CONFIG_ENABLE_EPAPER
#define ENABLE_EPAPER
#endif

/* low voltage protect */
#define VOLTAGE_PROTECT_THRESHOLD  (2800)  /* 2800 mV */

static float supply_voltage = 0;
static float humidity[READ_SENSOR_VALUE_NUM]={0};
static float temperature[READ_SENSOR_VALUE_NUM]={0};
static float luminance[READ_SENSOR_VALUE_NUM]={0};

static void low_vol_protect_task(void *arg)
{
    /* initialize ADC */
    adc_init();

    while(1) {
        uint32_t voltage_mv = adc_get_supply_voltage();
        supply_voltage = (float)voltage_mv / 1000;
        /* if supply voltage is lower than voltage protect threshold, enter deep-sleep mode */
        if (voltage_mv < VOLTAGE_PROTECT_THRESHOLD) {
            ESP_LOGI(TAG, "supply voltage is too low (<2.8V), enter deep-sleep");
            vTaskDelay(1000 / portTICK_RATE_MS);
            esp_sleep_enable_timer_wakeup(3 * 1000 * 1000);
            esp_deep_sleep_start();
        }
        vTaskDelay(1000 / portTICK_RATE_MS);
    }
}

static void low_vol_protect_start()
{
    xTaskCreate(low_vol_protect_task, "low_vol_protect_task", 2048, NULL, 5, NULL);
}

esp_err_t lowpower_evb_init_cb()
{
    low_vol_protect_start();
    
    /* initialize wifi config start button */
    button_init();
    /* initialize wifi and network led */
    status_led_init();

#ifdef ENABLE_EPAPER
    /* initialize epaper */
    epaper_display_init();
#endif
    
#ifdef ULP_WAKE_UP
    rtc_sensor_power_on();
    vTaskDelay(10 / portTICK_RATE_MS);  // 10 ms delay after power on
    rtc_sensor_io_init();
#endif

#ifdef NOT_ULP_WAKE_UP
    sensor_power_on();
    vTaskDelay(10 / portTICK_RATE_MS);  // 10 ms delay after power on
    sensor_init();
#endif

    return ESP_OK;
}

esp_err_t wifi_connect_cb(void)
{
    wifi_config_t wifi_config;
    esp_err_t ret = ESP_FAIL;

#ifdef USE_FIXED_ROUTER
    sprintf((char*)wifi_config.sta.ssid, "%s", FIXED_ROUTER_SSID);
    sprintf((char*)wifi_config.sta.password, "%s", FIXED_ROUTER_PASSWORD);
    iot_wifi_setup(WIFI_MODE_STA);
    wifi_led_slow_blink();
    ESP_LOGI(TAG, "connenct to fixed router, SSID:%s, PASSWORD:%s", wifi_config.sta.ssid, wifi_config.sta.password);
    ret = iot_wifi_connect((char*)(wifi_config.sta.ssid),  (char*)(wifi_config.sta.password), 20000 / portTICK_RATE_MS);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "connect wifi timeout");
        return ESP_FAIL;
    }
#endif

#ifdef USE_ESP_TOUCH_CONFIG
    ret = lowpower_evb_get_wifi_config(&wifi_config);
    ESP_LOGD(TAG, "iot_param_load status:%d", ret);

    if (ret != ESP_OK) {
        ESP_LOGI(TAG, "start smart config");
        wifi_led_quick_blink();
        lowpower_evb_wifi_config();
    } else {
        ESP_LOGI(TAG, "read wifi config from flash, SSID:%s, PASSWORD:%s", wifi_config.sta.ssid, wifi_config.sta.password);
        iot_wifi_setup(WIFI_MODE_STA);
        wifi_led_slow_blink();
        ret = iot_wifi_connect((char*)(wifi_config.sta.ssid),  (char*)(wifi_config.sta.password), 20000 / portTICK_RATE_MS);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "connect wifi timeout");
            return ESP_FAIL;
        }
    }
#endif

    wifi_led_on();
    ESP_LOGI(TAG, "connected with AP");

    return ESP_OK;
}

esp_err_t get_data_by_cpu_cb(void)
{
    for (int i=0; i<READ_SENSOR_VALUE_NUM; i++) {
        humidity[i] = sensor_hts221_get_hum();
        temperature[i] = sensor_hts221_get_temp();
        luminance[i] = sensor_bh1750_get_lum();
        vTaskDelay(100 / portTICK_RATE_MS);
    }

    for (int i=0; i<READ_SENSOR_VALUE_NUM; i++) {
        ESP_LOGI(TAG, "humidity[%d] = %2.1f", i, humidity[i]);
    }

    for (int i=0; i<READ_SENSOR_VALUE_NUM; i++) {
        ESP_LOGI(TAG, "temperature[%d] = %2.1f", i, temperature[i]);
    }
    
    for (int i=0; i<READ_SENSOR_VALUE_NUM; i++) {
        ESP_LOGI(TAG, "luminance[%d] = %2.1f", i, luminance[i]);
    }

    return ESP_OK;
}

esp_err_t ulp_program_init_cb(void)
{
    init_ulp_program();
    return ESP_OK;
}

esp_err_t get_data_from_ulp_cb(void)
{
    get_ulp_hts221_humidity(humidity, READ_SENSOR_VALUE_NUM);
    get_ulp_hts221_temperature(temperature, READ_SENSOR_VALUE_NUM);
    get_ulp_bh1750_luminance(luminance, READ_SENSOR_VALUE_NUM);
 
    for (int i=0; i<READ_SENSOR_VALUE_NUM; i++) {
        ESP_LOGI(TAG, "humidity[%d] = %2.1f", i, humidity[i]);
    }

    for (int i=0; i<READ_SENSOR_VALUE_NUM; i++) {
        ESP_LOGI(TAG, "temperature[%d] = %2.1f", i, temperature[i]);
    }
    
    for (int i=0; i<READ_SENSOR_VALUE_NUM; i++) {
        ESP_LOGI(TAG, "luminance[%d] = %2.1f", i, luminance[i]);
    }

    return ESP_OK;
}

static uint32_t convert_data_to_string(char *data_name, float *data, uint32_t data_num, char *data_str)
{
    char *data_str_tmp = data_str;
    data_str_tmp += sprintf(data_str_tmp, "%s:", data_name);

    for (int i=0; i<data_num; i++) {
        data_str_tmp += sprintf(data_str_tmp, " %2.1f", data[i]);
    }
    
    data_str_tmp += sprintf(data_str_tmp, "\n");
    
    return (strlen(data_str));
}

esp_err_t send_data_to_server_cb(void)
{
    CTcpConn client;
    network_led_quick_blink();

    client.SetTimeout(10*1000);
    if (client.Connect(SERVER_IP, SERVER_PORT) < 0) {
        ESP_LOGE(TAG, "fail to connect sever!");
        return ESP_FAIL;
    }
    ESP_LOGI(TAG, "TCP connected");
    network_led_on();

    uint32_t upload_data_size = 0;
    char *upload_data = (char *)malloc(UPLOAD_DATA_SIZE);
    memset(upload_data, 0, UPLOAD_DATA_SIZE);
    upload_data_size = convert_data_to_string((char*)"Humidity", humidity, READ_SENSOR_VALUE_NUM, upload_data);
    ESP_LOGI(TAG, "upload data:%s", upload_data);
    
    if (client.Write((const uint8_t*)upload_data, upload_data_size) < 0) {
        goto UPLOAD_FIAL;
    }

    memset(upload_data, 0, UPLOAD_DATA_SIZE);
    upload_data_size = convert_data_to_string((char*)"Temperature", temperature, READ_SENSOR_VALUE_NUM, upload_data);
    ESP_LOGI(TAG, "upload data:%s", upload_data);

    if (client.Write((const uint8_t*)upload_data, upload_data_size) < 0) {
        goto UPLOAD_FIAL;
    }

    memset(upload_data, 0, UPLOAD_DATA_SIZE);
    upload_data_size = convert_data_to_string((char*)"Luminance", luminance, READ_SENSOR_VALUE_NUM, upload_data);
    ESP_LOGI(TAG, "upload data:%s", upload_data);

    if (client.Write((const uint8_t*)upload_data, upload_data_size) < 0) {
        goto UPLOAD_FIAL;
    }

    ESP_LOGI(TAG, "TCP send data done");
    ESP_LOGI(TAG, "TCP disconnect");
    client.Disconnect();
    if (upload_data) {
        free(upload_data);
    }
    return ESP_OK;

UPLOAD_FIAL:
    ESP_LOGE(TAG, "fail to send data to server");
    ESP_LOGI(TAG, "TCP disconnect");
    client.Disconnect();
    if (upload_data) {
        free(upload_data);
    }
    return ESP_FAIL;
}

esp_err_t send_data_done_cb(void)
{
    supply_voltage = adc_get_supply_voltage();
    ESP_LOGI(TAG, "supply voltage:%2.1fV", supply_voltage);

#ifdef ENABLE_EPAPER
    epaper_show_page(humidity[READ_SENSOR_VALUE_NUM-1], temperature[READ_SENSOR_VALUE_NUM-1], 
                     luminance[READ_SENSOR_VALUE_NUM-1], supply_voltage);
#endif

    return ESP_OK;
}

esp_err_t start_deep_sleep_cb(void)
{
    ESP_LOGI(TAG, "start deep sleep");
    
    /* the GPIO led used is rtc IO, and its status is not fixed in deep-sleep mode,
     * so we need to hold the io level before enter deep-sleep mode.
     */
    led_io_rtc_hold_en();

#ifdef ULP_WAKE_UP
    ESP_LOGI(TAG, "use ulp or gpio%d to wake up", GPIO_WAKE_UP_IO);
    esp_sleep_enable_ext1_wakeup(1ULL<<GPIO_WAKE_UP_IO, ESP_EXT1_WAKEUP_ALL_LOW);
    set_ulp_read_value_number(READ_SENSOR_VALUE_NUM);
    set_ulp_read_interval(READ_SENSOR_INTERVAL);
    start_ulp_program();
    ESP_ERROR_CHECK(esp_sleep_enable_ulp_wakeup());

    esp_deep_sleep_start();
#endif

#ifdef GPIO_WAKE_UP
    ESP_LOGI(TAG, "use gpio%d to wake up", GPIO_WAKE_UP_IO);
    esp_sleep_enable_ext0_wakeup((gpio_num_t)GPIO_WAKE_UP_IO, GPIO_WAKE_UP_LEVEL);
    esp_deep_sleep_start();
#endif

#ifdef TIMER_WAKE_UP
    ESP_LOGI(TAG, "use timer to wake up, wake up after %d us", WAKEUP_TIME_US);
    esp_sleep_enable_timer_wakeup(WAKEUP_TIME_US);
    esp_deep_sleep_start();
#endif

    return ESP_OK;
}


