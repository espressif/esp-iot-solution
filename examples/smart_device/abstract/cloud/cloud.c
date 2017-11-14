/*
  * ESPRESSIF MIT License
  *
  * Copyright (c) 2017 <ESPRESSIF SYSTEMS (SHANGHAI) PTE LTD>
  *
  * Permission is hereby granted for use on ESPRESSIF SYSTEMS products only, in which case,
  * it is free of charge, to any person obtaining a copy of this software and associated
  * documentation files (the "Software"), to deal in the Software without restriction, including
  * without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense,
  * and/or sell copies of the Software, and to permit persons to whom the Software is furnished
  * to do so, subject to the following conditions:
  *
  * The above copyright notice and this permission notice shall be included in all copies or
  * substantial portions of the Software.
  *
  * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
  * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS
  * FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR
  * COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER
  * IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
  * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
  *
  */

#include "esp_system.h"
#include "cloud.h"
#include "esp_log.h"
#include "esp_wifi.h"
#include "esp_event_loop.h"
#include "iot_button.h"
#include "light_device.h"

#ifdef CLOUD_ALINK
#include "esp_alink.h"
#include "alink_product.h"
#include "cJSON.h"
#include "esp_json_parser.h"
#include "alink_export.h"
#include "esp_spi_flash.h"
static const char* TAG = "cloud_alink";
#endif

#ifdef CLOUD_JOYLINK
#include "jd_innet.h"
#include "esp_joylink.h"
#include "joylink_config.h"
static const char* TAG = "cloud_joylink";
#endif

light_dev_handle_t g_light_handle;
typedef esp_err_t (* cloud_event_cb_t)(cloud_event_t);

#ifdef CLOUD_JOYLINK
static char *device_attr[5] = { "power", "brightness", "colortemp", "mode", "color"};
/*
 * brightness  |  int     |  0 ~ 100
 * color       |  string  |  
 * colortemp   |  int     |  3000 ~ 6500
 * mode        |  int     |  0/1/2/3/4
 * power       |  int     |  0/1
*/
const char *joylink_json_upload_str = \
"{\
\"code\":%d,\
\"data\":[\
{\"current_value\": \"%d\",\"stream_id\": \"power\"},\
{\"current_value\": \"%d\",\"stream_id\": \"brightness\"},\
{\"current_value\": \"%d\",\"stream_id\": \"colortemp\"},\
{\"current_value\": \"%d\",\"stream_id\": \"mode\"},\
{\"current_value\": \"%s\",\"stream_id\": \"color\"}\
]}";
#endif

#ifdef CLOUD_ALINK
typedef struct  light_dev {
    uint32_t errorcode;
    uint32_t power;
    uint32_t work_mode;
    uint32_t hue;
    uint32_t saturation;
    uint32_t luminance;
} light_dev_t;

static light_dev_t g_light_info = {
    .errorcode  = 0x00,
    .power      = 0x01,
    .work_mode  = 0x02,
    .hue        = 0x10,
    .saturation = 0x20,
    .luminance  = 0x50,
};

/**
 * @brief  In order to simplify the analysis of json package operations,
 * use the package alink_json_parse, you can also use the standard cJson data analysis
 */
static alink_err_t device_data_parse(const char *json_str, const char *key, uint32_t *value)
{
    char sub_str[64] = {0};
    char value_tmp[8] = {0};

    if (esp_json_parse(json_str, key, sub_str) < 0) {
        return ALINK_ERR;
    }

    if (esp_json_parse(sub_str, "value", value_tmp) < 0) {
        return ALINK_ERR;
    }

    *value = atoi(value_tmp);
    return ALINK_ERR;
}

static alink_err_t device_data_pack(const char *json_str, const char *key, uint32_t value)
{
    char sub_str[64] = {0};

    if (esp_json_pack(sub_str, "value", value) < 0) {
        return ALINK_ERR;
    }

    if (esp_json_pack(json_str, key, sub_str) < 0) {
        return ALINK_ERR;
    }

    return ALINK_OK;
}

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

    device_data_pack(up_cmd, "Switch", g_light_info.power);
    device_data_pack(up_cmd, "WorkMode", g_light_info.work_mode);
    device_data_pack(up_cmd, "Hue", g_light_info.hue);
    device_data_pack(up_cmd, "Saturation", g_light_info.hue);
    device_data_pack(up_cmd, "Luminance", g_light_info.luminance);

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
        light_net_status_write(g_light_handle, LIGHT_CLOUD_CONNECTED);
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
        light_net_status_write(g_light_handle, LIGHT_STA_DISCONNECTED);
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

#ifdef CLOUD_JOYLINK
static esp_err_t joylink_event_handler(void *ctx, system_event_t *event)
{
    switch(event->event_id) {
    case SYSTEM_EVENT_STA_START:
        // esp_wifi_connect();
        break;
    case SYSTEM_EVENT_STA_GOT_IP:
        break;
    case SYSTEM_EVENT_STA_DISCONNECTED:
        /* This is a workaround as ESP32 WiFi libs don't currently
           auto-reassociate. */
        esp_wifi_connect();
        break;
    default:
        break;
    }
    return ESP_OK;
}

static void initialise_wifi(void)
{
    tcpip_adapter_init();
    //ESP_ERROR_CHECK( esp_event_loop_init(joylink_event_handler, NULL) );
    ESP_ERROR_CHECK( esp_event_loop_init(NULL, NULL) );
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK( esp_wifi_init(&cfg) );
    ESP_ERROR_CHECK( esp_wifi_set_storage(WIFI_STORAGE_RAM) );

    ESP_ERROR_CHECK( esp_wifi_set_mode(WIFI_MODE_NULL) );
    ESP_ERROR_CHECK( esp_wifi_start() );
}

static void initialise_key(void)
{
    button_handle_t btn_handle = iot_button_create(CONFIG_JOYLINK_SMNT_BUTTON_NUM, BUTTON_ACTIVE_LOW, BUTTON_SINGLE_TRIGGER, 1);
    iot_button_add_cb(btn_handle, BUTTON_PUSH_CB, joylink_button_smnt_tap_cb, "PUSH", 50 / portTICK_PERIOD_MS);

    btn_handle = iot_button_create(CONFIG_JOYLINK_RESET_BUTTON_NUM, BUTTON_ACTIVE_LOW, BUTTON_SINGLE_TRIGGER, 1);
    iot_button_add_cb(btn_handle, BUTTON_PUSH_CB, joylink_button_reset_tap_cb, "PUSH", 50 / portTICK_PERIOD_MS);
}
#endif

esp_err_t cloud_init()
{
    g_light_handle = light_init();

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


#ifdef CLOUD_JOYLINK
    initialise_wifi();
    initialise_key();
    printf("mode: json, free_heap: %u\n", esp_get_free_heap_size());
    joylink_info_t product_info = {
        .innet_aes_key       = JOYLINK_AES_KEY,
        .jlp.version         = JOYLINK_VERSION,
        .jlp.accesskey       = JOYLINK_ACCESSKEY,//NDJY9396Z9P4KNDU
        .jlp.localkey        = JOYLINK_LOCAL_KEY,
        .jlp.feedid          = JOYLINK_FEEDID,
        .jlp.devtype         = JOYLINK_DEVTYPE,
        .jlp.joylink_server  = JOYLINK_SERVER,
        .jlp.server_port     = JOYLINK_SERVER_PORT,
        .jlp.CID             = JOYLINK_CID,
        .jlp.firmwareVersion = JOYLINK_FW_VERSION,
        .jlp.modelCode       = JOYLINK_MODEL_CODE,
        .jlp.uuid            = JOYLINK_UUID,
        .jlp.lancon          = JOYLINK_LAN_CTRL,
        .jlp.cmd_tran_type   = JOYLINK_CMD_TYPE
    };
    uint8_t dev_mac[6];
    char dev_mac_str[20];
    memset(dev_mac_str, 0, 20);
    esp_wifi_get_mac(ESP_IF_WIFI_STA, dev_mac);
    sprintf(dev_mac_str, ""MACSTR"", MAC2STR(dev_mac));
    strcpy(product_info.jlp.mac, dev_mac_str);
    printf("*********************************\r\n");
    printf("*         PRODUCT INFO          *\r\n");
    printf("*********************************\r\n");
    printf("UUID     : %s\r\n", product_info.jlp.uuid);
    printf("mac      : %s\r\n", product_info.jlp.mac);
    printf("type     : %d\r\n", product_info.jlp.devtype);
    printf("version  : %d\r\n", product_info.jlp.version);
    printf("fw_ver   : %s\r\n", product_info.jlp.firmwareVersion);
    printf("model    : %s\r\n", product_info.jlp.modelCode);
    printf("tran type: %d\r\n", product_info.jlp.cmd_tran_type);
    ESP_ERROR_CHECK( esp_joylink_event_init((joylink_event_cb_t)cb) );
    esp_joylink_init(&product_info);
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
        device_data_parse(down_cmd, "ErrorCode", &(g_light_info.errorcode));
        device_data_parse(down_cmd, "Switch", &(g_light_info.power));
        device_data_parse(down_cmd, "WorkMode", &(g_light_info.work_mode));
        device_data_parse(down_cmd, "Hue", &(g_light_info.hue));
        device_data_parse(down_cmd, "Saturation", &(g_light_info.saturation));
        device_data_parse(down_cmd, "Luminance", &(g_light_info.luminance));

        ALINK_LOGI("read: errorcode:%d, switch: %d, work_mode: %d, hue: %d, saturation: %d, luminance: %d",
                   g_light_info.errorcode, g_light_info.power, g_light_info.work_mode,
                   g_light_info.hue, g_light_info.saturation, g_light_info.luminance);

        if (g_light_info.power == 0) {
            light_set(g_light_handle, 0, 0, 0);
        } else {
            light_set(g_light_handle, g_light_info.hue, g_light_info.saturation, g_light_info.luminance);
        }
    }

    free(down_cmd);
#endif
    
#ifdef CLOUD_JOYLINK
    char *down_cmd = (char *)malloc(JOYLINK_DATA_LEN);
    esp_joylink_read(down_cmd, JOYLINK_DATA_LEN, portMAX_DELAY);
    int i, is_crtl = 0;
    int valueLen = 0;
    char *valueStr = NULL;
    char data_buff[30];

    char *pCmd = down_cmd;

    for (i = 0; i < 5; i++) {
        valueStr = joylink_json_get_value_by_id(pCmd, strlen(pCmd), device_attr[i], &valueLen);
        if (valueStr && valueLen > 0) {
            memset(data_buff, 0, 30);
            memcpy(data_buff, valueStr, valueLen);
            ESP_LOGI(TAG, "valueStr:%s, attrLen:%d",data_buff, valueLen);
            is_crtl ++;

            switch (i) {
            case 0:
                virtual_dev->power = atoi(data_buff);
                break;
            case 1:
                virtual_dev->light_value= atoi(data_buff);
                break;
            case 2:
                virtual_dev->temp_value = atoi(data_buff) / 100;
                break;
            case 3:
                virtual_dev->work_mode= atoi(data_buff);
                break;
            case 4:
                virtual_dev->time_delay = 0;
                //strcpy(virtual_device.color, data_buff);
                break;
            default:
                break;
            }
       }
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

#ifdef CLOUD_JOYLINK
    char *up_cmd = (char *)malloc(JOYLINK_DATA_LEN);
    memset(up_cmd, 0, JOYLINK_DATA_LEN);
    sprintf(up_cmd, joylink_json_upload_str, JOYLINK_RET_JSON_OK, virtual_device.power, virtual_device.light_value,
        virtual_device.temp_value, virtual_device.work_mode, "red");
    int ret = esp_joylink_write(up_cmd, strlen(up_cmd)+1, 500 / portTICK_PERIOD_MS);
    free(up_cmd);
    return ret;
#endif
    return ESP_OK;
}

int cloud_sys_net_is_ready(void)
{
#ifdef CLOUD_ALINK
    return platform_sys_net_is_ready();
#endif
    return -1;
}
