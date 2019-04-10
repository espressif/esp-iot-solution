/* Smart device Example

   For other examples please check:
   https://github.com/espressif/esp-iot-solution/tree/master/examples

   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
 */

#include "esp_system.h"
#include "cloud.h"
#include "esp_log.h"
#include "esp_wifi.h"
#include "esp_event_loop.h"
#include "iot_button.h"
#include "light_device.h"
#include "device.h"

#ifdef CLOUD_ALINK
#include "esp_alink.h"
#include "alink_product.h"
#include "cJSON.h"
#include "esp_json_parser.h"
#include "alink_export.h"
#include "esp_spi_flash.h"
static const char* TAG = "cloud_alink";
#endif

#ifdef CLOUD_ALINK

/**
 * @brief When the service received errno a jump that is complete activation,
 *        activation of the order need to modify the specific equipment
 */
static alink_err_t alink_activate_device()
{
    alink_err_t ret = 0;
    const char *activate_data = NULL;

    activate_data = "{\"ErrorCode\": { \"value\": \"1\" }}";
    ret = alink_write(activate_data, strlen(activate_data) + 1, 200);
    ALINK_ERROR_CHECK(ret < 0, ALINK_ERR, "alink_write, ret: %d", ret);

    activate_data = "{\"ErrorCode\": { \"value\": \"0\" }}";
    ret = alink_write(activate_data, strlen(activate_data) + 1, 200);
    ALINK_ERROR_CHECK(ret < 0, ALINK_ERR, "alink_write, ret: %d", ret);

    return ALINK_OK;
}

/**
 * @brief  The alink protocol specifies that the state of the device must be
 *         proactively attached to the Ali server
 */
static alink_err_t alink_report_data(int ms_wait)
{
    alink_err_t ret = 0;
    char *up_cmd = (char *)calloc(1, ALINK_DATA_LEN);

    cloud_device_read(up_cmd);
    ALINK_LOGI("cloud write: %s", up_cmd);

    ret = alink_write(up_cmd, strlen(up_cmd) + 1, ms_wait);
    free(up_cmd);

    if (ret < 0) {
        ALINK_LOGW("alink_write is err");
        return ALINK_ERR;
    }

    return ALINK_OK;
}

static alink_err_t alink_event_handler(alink_event_t event)
{
    switch (event) {
    case ALINK_EVENT_CLOUD_CONNECTED:
        ALINK_LOGD("Alink cloud connected!");
        cloud_device_net_status_write(CLOUD_CONNECTED);
        alink_report_data(500 / portTICK_RATE_MS);

        /*!< zero configuration need to open the softap */
        wifi_mode_t wifi_mode = 0;
        ESP_ERROR_CHECK(esp_wifi_get_mode(&wifi_mode));

        if (wifi_mode != WIFI_MODE_APSTA) {
            ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_APSTA));
        }
        
        break;

    case ALINK_EVENT_CLOUD_DISCONNECTED:
        ALINK_LOGD("Alink cloud disconnected!");
        cloud_device_net_status_write(CLOUD_DISCONNECTED);
        break;

    case ALINK_EVENT_GET_DEVICE_DATA:
        ALINK_LOGD("The cloud initiates a query to the device");
        break;

    case ALINK_EVENT_SET_DEVICE_DATA:
        ALINK_LOGD("The cloud is set to send instructions");
        break;

    case ALINK_EVENT_POST_CLOUD_DATA:
        ALINK_LOGD("The device post data success!");
        break;

    case ALINK_EVENT_WIFI_DISCONNECTED:
        cloud_device_net_status_write(STA_DISCONNECTED);
        ALINK_LOGD("Wifi disconnected");
        break;

    case ALINK_EVENT_CONFIG_NETWORK:
        ALINK_LOGD("Enter the network configuration mode");
        break;

    case ALINK_EVENT_UPDATE_ROUTER:
        ALINK_LOGD("Requests update router");
        alink_update_router();
        break;

    case ALINK_EVENT_FACTORY_RESET:
        ALINK_LOGD("Requests factory reset");
        alink_factory_setting();
        break;

    case ALINK_EVENT_ACTIVATE_DEVICE:
        ALINK_LOGD("Requests activate device");
        alink_activate_device();
        break;

    default:
        break;
    }

    return ALINK_OK;
}

/**
 * @brief Too much serial print information will not be able to pass high-frequency
 *        send and receive data test
 *
 * @Note When GPIO2 is connected to 3V3 and restarts the device, the log level will be modified
 */
void reduce_serial_print()
{
    gpio_config_t io_conf = {
        .intr_type = GPIO_INTR_ANYEDGE,  // interrupt of rising edge
        .pin_bit_mask = 1 << GPIO_NUM_2,  // bit mask of the pins, use GPIO2 here
        .mode = GPIO_MODE_INPUT,  // set as input mode
        .pull_up_en = GPIO_PULLUP_ENABLE,  // enable pull-up mode
    };

    ESP_ERROR_CHECK(gpio_config(&io_conf));

    if (!gpio_get_level(GPIO_NUM_2)) {
        ALINK_LOGI("*********************************");
        ALINK_LOGI("*       SET LOGLEVEL INFO       *");
        ALINK_LOGI("*********************************");
        alink_set_loglevel(ALINK_LL_INFO);
    }
}
#endif

esp_err_t cloud_init(SemaphoreHandle_t xSemWriteInfo)
{
    cloud_device_init(xSemWriteInfo);

#ifdef CLOUD_ALINK
    esp_chip_info_t chip_info;
    esp_chip_info(&chip_info);

    ALINK_LOGI("================= SYSTEM INFO ================");
    ALINK_LOGI("compile time     : %s %s", __DATE__, __TIME__);
    ALINK_LOGI("free heap        : %dB", esp_get_free_heap_size());
    ALINK_LOGI("idf version      : %s", esp_get_idf_version());
    ALINK_LOGI("CPU cores        : %d", chip_info.cores);
    ALINK_LOGI("chip name        : %s", ALINK_CHIPID);
    ALINK_LOGI("modle name       : %s", ALINK_MODULE_NAME);
    ALINK_LOGI("function         : WiFi%s%s",
               (chip_info.features & CHIP_FEATURE_BT) ? "/BT" : "",
               (chip_info.features & CHIP_FEATURE_BLE) ? "/BLE" : "");
    ALINK_LOGI("silicon revision : %d", chip_info.revision);
    ALINK_LOGI("flash            : %dMB %s", spi_flash_get_chip_size() / (1024 * 1024),
               (chip_info.features & CHIP_FEATURE_EMB_FLASH) ? "embedded" : "external");

#ifdef CONFIG_ALINK_VERSION_SDS
    ALINK_LOGI("alink version    : esp32-alink_sds\n");
#else
    ALINK_LOGI("alink version    : esp32-alink_embed\n");
#endif

    tcpip_adapter_init();
    ESP_ERROR_CHECK(esp_event_loop_init(NULL, NULL));
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));
    ESP_ERROR_CHECK(esp_wifi_set_storage(WIFI_STORAGE_RAM));

    /**
     * @brief You can use other trigger mode, to trigger the distribution network, activation and other operations
     */
    extern void alink_key_trigger(void *arg);
    xTaskCreate(alink_key_trigger, "alink_key_trigger", 1024 * 2, NULL, 10, NULL);

    const alink_product_t product_info = {
        .name           = ALINK_INFO_NAME,
        /*!< Product version number, ota upgrade need to be modified */
        .version        = ALINK_INFO_VERSION,
        .model          = ALINK_INFO_MODEL,
        /*!< The Key-value pair used in the product */
        .key            = ALINK_INFO_KEY,
        .secret         = ALINK_INFO_SECRET,
        /*!< The Key-value pair used in the sandbox environment */
        .key_sandbox    = ALINK_INFO_KEY_SANDBOX,
        .secret_sandbox = ALINK_INFO_SECRET_SANDBOX,

#ifdef CONFIG_ALINK_VERSION_SDS
        /**
         * @brief As a unique identifier for the sds device
         */
        .key_device     = ALINK_INFO_KEY_DEVICE,
        .secret_device  = ALINK_INFO_SECRET_DEVICE,
#endif
    };

    ESP_ERROR_CHECK(alink_init(&product_info, alink_event_handler));
    reduce_serial_print();
#endif
    return ESP_OK;
}

esp_err_t cloud_read(int ms_wait)
{
#ifdef CLOUD_ALINK
    char *down_cmd = (char *)malloc(ALINK_DATA_LEN);

    if (alink_read(down_cmd, ALINK_DATA_LEN, ms_wait) < 0) {
        ALINK_LOGW("alink_read is err");
        return ESP_FAIL;
    }

    ALINK_LOGI("cloud read: %s", down_cmd);
    char method_str[32] = {0};
    if (esp_json_parse(down_cmd, "method", method_str) < 0) {
        ALINK_LOGW("alink_json_parse, is err");
        return ESP_FAIL;
    }

    if (!strcmp(method_str, "getDeviceStatus")) {
        alink_report_data(500 / portTICK_RATE_MS);
        return ESP_FAIL;
    } else if (!strcmp(method_str, "setDeviceStatus")) {
        ALINK_LOGV("setDeviceStatus: %s", down_cmd);
        cloud_device_write(down_cmd);
    }

    free(down_cmd);
#endif

    return ESP_OK;
}

esp_err_t cloud_write(int ms_wait)
{
#ifdef CLOUD_ALINK
    esp_err_t ret = alink_report_data(ms_wait);
    return ret;
#endif
    return ESP_OK;
}

