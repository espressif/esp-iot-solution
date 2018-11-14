/* Smart device Example

   For other examples please check:
   https://github.com/espressif/esp-iot-solution/tree/master/examples

   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
 */

#include "esp_err.h"
#include "esp_system.h"
#include "esp_alink.h"
#include "esp_json_parser.h"
#include "parse.h"

static const char* TAG = "cloud_device_parse";

/**
 * @brief  In order to simplify the analysis of json package operations,
 * use the package alink_json_parse, you can also use the standard cJson data analysis
 */
static esp_err_t device_data_parse(const char *json_str, const char *key, uint32_t *value)
{
    char sub_str[64] = {0};
    char value_tmp[8] = {0};

    if (esp_json_parse(json_str, key, sub_str) < 0) {
        return ESP_FAIL;
    }

    if (esp_json_parse(sub_str, "value", value_tmp) < 0) {
        return ESP_FAIL;
    }

    *value = atoi(value_tmp);

    return ESP_FAIL;
}

static esp_err_t device_data_pack(const char *json_str, const char *key, uint32_t value)
{
    char sub_str[64] = {0};

    if (esp_json_pack(sub_str, "value", value) < 0) {
        return ESP_FAIL;
    }

    if (esp_json_pack(json_str, key, sub_str) < 0) {
        return ESP_FAIL;
    }

    return ESP_OK;
}

#ifdef SMART_LIGHT_DEVICE
esp_err_t cloud_device_parse(char *down_cmd, smart_light_dev_t *light_info)
{
    device_data_parse(down_cmd, "ErrorCode", &(light_info->errorcode));
    device_data_parse(down_cmd, "Switch", &(light_info->on_off));
    device_data_parse(down_cmd, "WorkMode", &(light_info->work_mode));
    device_data_parse(down_cmd, "Luminance", &(light_info->luminance));        
    device_data_parse(down_cmd, "ColorTemperature", &(light_info->color_temp));
    device_data_parse(down_cmd, "Red", &(light_info->red));
    device_data_parse(down_cmd, "Green", &(light_info->green));
    device_data_parse(down_cmd, "Blue", &(light_info->blue));

    ESP_LOGI(TAG, "device write: errorcode:%d, switch:%d, work_mode:%d, luminance:%d, color_temperature:%d, red:%d, green:%d, blue:%d",
               light_info->errorcode, light_info->on_off, light_info->work_mode,
               light_info->luminance, light_info->color_temp, light_info->red, light_info->green, light_info->blue);

    return ESP_OK;
}

esp_err_t cloud_device_pack(char *up_cmd, smart_light_dev_t *light_info)
{    
    ESP_LOGI(TAG, "device read: errorcode:%d, switch:%d, work_mode:%d, luminance:%d, color_temperature:%d, red:%d, green:%d, blue:%d",
               light_info->errorcode, light_info->on_off, light_info->work_mode,
               light_info->luminance, light_info->color_temp, light_info->red, light_info->green, light_info->blue);

    device_data_pack(up_cmd, "Switch", light_info->on_off);
    device_data_pack(up_cmd, "WorkMode", light_info->work_mode);
    device_data_pack(up_cmd, "Luminance", light_info->luminance);
    device_data_pack(up_cmd, "ColorTemperature", light_info->color_temp);
    device_data_pack(up_cmd, "Red", light_info->red);
    device_data_pack(up_cmd, "Green", light_info->green);
    device_data_pack(up_cmd, "Blue", light_info->blue);

    return ESP_OK;
}
#endif

#ifdef SMART_PLUG_DEVICE
esp_err_t cloud_device_parse(char *down_cmd, smart_plug_dev_t *plug_info)
{
    device_data_parse(down_cmd, "ErrorCode", &(plug_info->errorcode));
    device_data_parse(down_cmd, "Switch", &(plug_info->on_off));
    device_data_parse(down_cmd, "Power", &(plug_info->power));
    device_data_parse(down_cmd, "RmsCurrent", &(plug_info->rms_current));
    device_data_parse(down_cmd, "RmsVoltage", &(plug_info->rms_voltage));
    device_data_parse(down_cmd, "SwitchMultiple_1", &(plug_info->switch_multiple_1));
    device_data_parse(down_cmd, "SwitchMultiple_2", &(plug_info->switch_multiple_2));
    device_data_parse(down_cmd, "SwitchMultiple_3", &(plug_info->switch_multiple_3));

    ESP_LOGI(TAG, "device write: errorcode:%d, switch: %d, power: %d, rms_current: %d, rms_voltage: %d, switch_multiple_1: %d, switch_multiple_2: %d, switch_multiple_3: %d",
               plug_info->errorcode, plug_info->on_off, plug_info->power, plug_info->rms_current, 
               plug_info->rms_voltage, plug_info->switch_multiple_1, plug_info->switch_multiple_2, plug_info->switch_multiple_3);

    return ESP_OK;
}

esp_err_t cloud_device_pack(char *up_cmd, smart_plug_dev_t *plug_info)
{
    ESP_LOGI(TAG, "device read: errorcode:%d, switch: %d, power: %d, rms_current: %d, rms_voltage: %d, switch_multiple_1: %d, switch_multiple_2: %d, switch_multiple_3: %d",
               plug_info->errorcode, plug_info->on_off, plug_info->power, plug_info->rms_current, 
               plug_info->rms_voltage, plug_info->switch_multiple_1, plug_info->switch_multiple_2, plug_info->switch_multiple_3);

    device_data_pack(up_cmd, "Switch", plug_info->on_off);
    device_data_pack(up_cmd, "Power", plug_info->power);
    device_data_pack(up_cmd, "RmsCurrent", plug_info->rms_current);
    device_data_pack(up_cmd, "RmsVoltage", plug_info->rms_voltage);
    device_data_pack(up_cmd, "SwitchMultiple_1", plug_info->switch_multiple_1);
    device_data_pack(up_cmd, "SwitchMultiple_2", plug_info->switch_multiple_2);
    device_data_pack(up_cmd, "SwitchMultiple_3", plug_info->switch_multiple_3);

    return ESP_OK;
}
#endif


