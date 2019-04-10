/* Smart device Example

   For other examples please check:
   https://github.com/espressif/esp-iot-solution/tree/master/examples

   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
 */

#include <stdlib.h>
#include "iot_light.h"
#include "light_device.h"
#include "light_config.h"
#include "iot_param.h"
#include "iot_button.h"

typedef struct {
    int ledc_channel;
    int ledc_gpio;
} light_conf_t;

static const light_conf_t LIGHT_PWM_UNIT[IOT_LIGHT_CHANNEL_NUM] = {
    {
        .ledc_channel = IOT_LIGHT_PWM_U0_LEDC_CHANNEL,
        .ledc_gpio    = IOT_LIGHT_PWM_U0_LEDC_IO,
    },
#if IOT_LIGHT_CHANNEL_NUM > 1
    {
        .ledc_channel = IOT_LIGHT_PWM_U1_LEDC_CHANNEL,
        .ledc_gpio    = IOT_LIGHT_PWM_U1_LEDC_IO,
    },
#endif
#if IOT_LIGHT_CHANNEL_NUM > 2
    {
        .ledc_channel = IOT_LIGHT_PWM_U2_LEDC_CHANNEL,
        .ledc_gpio    = IOT_LIGHT_PWM_U2_LEDC_IO,
    },
#endif
#if IOT_LIGHT_CHANNEL_NUM > 3
    {
        .ledc_channel = IOT_LIGHT_PWM_U3_LEDC_CHANNEL,
        .ledc_gpio    = IOT_LIGHT_PWM_U3_LEDC_IO,
    },
#endif
#if IOT_LIGHT_CHANNEL_NUM > 4
    {
        .ledc_channel = IOT_LIGHT_PWM_U4_LEDC_CHANNEL,
        .ledc_gpio    = IOT_LIGHT_PWM_U4_LEDC_IO,
    },
#endif
};

typedef struct {
    uint32_t red;
    uint32_t green;
    uint32_t blue;
    uint32_t color_temp;
    uint32_t luminance;
} light_param_t;

typedef struct {
    light_handle_t light_handle;
    light_param_t light_param;
} light_device_t;

light_dev_handle_t light_init()
{
    light_device_t* light_dev = (light_device_t*)calloc(1, sizeof(light_device_t));
    light_handle_t light = iot_light_create(IOT_LIGHT_PWM_TIMER_IDX, IOT_LIGHT_PWM_SPEED_MODE, IOT_LIGHT_FREQ_HZ, IOT_LIGHT_CHANNEL_NUM, IOT_LIGHT_PWM_BIT_NUM);
    light_dev->light_handle = light;

    for(int i = 0; i< IOT_LIGHT_CHANNEL_NUM; i ++) {
        iot_light_channel_regist(light, i, LIGHT_PWM_UNIT[i].ledc_gpio, LIGHT_PWM_UNIT[i].ledc_channel);
    }

    if (iot_param_load(IOT_LIGHT_NAME_SPACE, IOT_LIGHT_PARAM_KEY, &(light_dev->light_param)) != ESP_OK) {
        light_set(light_dev, 0, 0, 0, 2700, 0);
    }
    else {
        light_set(light_dev, light_dev->light_param.red, light_dev->light_param.green, 
            light_dev->light_param.blue, light_dev->light_param.color_temp, light_dev->light_param.luminance);
    }

    light_net_status_write(light_dev, LIGHT_STA_DISCONNECTED);
    return light_dev;
}

static esp_err_t light_state_write(light_handle_t light_handle, uint32_t duty[])
{
    if (light_handle ==NULL) {
        return ESP_FAIL;
    }
    for (int i = 0; i < IOT_LIGHT_CHANNEL_NUM; i++) {
        iot_light_duty_write(light_handle, i, duty[i], LIGHT_DUTY_FADE_2S);
    }
    return ESP_OK;
}

esp_err_t light_set(light_dev_handle_t light_dev, uint32_t red, uint32_t green, uint32_t blue, uint32_t color_temp, uint32_t luminance)
{
    if (light_dev ==NULL) {
        return ESP_FAIL;
    }

    uint32_t duty_all[5] = {red * LIGHT_FULL_DUTY / 255, green * LIGHT_FULL_DUTY / 255, 
    blue * LIGHT_FULL_DUTY / 255, (color_temp-2700) * LIGHT_FULL_DUTY / (6500-2700), luminance * LIGHT_FULL_DUTY / 100};

    uint32_t duty[IOT_LIGHT_CHANNEL_NUM] = {0};
    for (int i = 0; i < IOT_LIGHT_CHANNEL_NUM; i++) {
        duty[i] = duty_all[i];
    }

    light_device_t* light_dev_tmp = (light_device_t*)light_dev;
    light_dev_tmp->light_param.red = red;
    light_dev_tmp->light_param.green= green;
    light_dev_tmp->light_param.blue = blue;
    light_dev_tmp->light_param.color_temp = color_temp;
    light_dev_tmp->light_param.luminance = luminance;

    iot_param_save(IOT_LIGHT_NAME_SPACE, IOT_LIGHT_PARAM_KEY, &light_dev_tmp->light_param, sizeof(light_param_t));
    return light_state_write(light_dev_tmp->light_handle, duty);
}

esp_err_t light_get(light_dev_handle_t light_dev, uint32_t *red, uint32_t *green, uint32_t *blue, uint32_t *color_temp, uint32_t *luminance)
{
    light_device_t* light_dev_tmp = (light_device_t*)light_dev;
    esp_err_t ret = iot_param_load(IOT_LIGHT_NAME_SPACE, IOT_LIGHT_PARAM_KEY, &(light_dev_tmp->light_param));

    if (ret == ESP_OK) {
        *red = light_dev_tmp->light_param.red;
        *green = light_dev_tmp->light_param.green;
        *blue = light_dev_tmp->light_param.blue;
        *color_temp = light_dev_tmp->light_param.color_temp;
        *luminance = light_dev_tmp->light_param.luminance;
    }

    return ret;
}

esp_err_t light_net_status_write(light_dev_handle_t light_dev, light_net_status_t net_status)
{
    if (light_dev ==NULL) {
        return ESP_FAIL;
    }
    
    light_device_t* light_dev_tmp = (light_device_t*)light_dev;
    switch (net_status) {
        case LIGHT_STA_DISCONNECTED:
            iot_light_breath_write(light_dev_tmp->light_handle, IOT_CHANNEL_ID_R, 4000);
            iot_light_duty_write(light_dev_tmp->light_handle, IOT_CHANNEL_ID_G, 0, LIGHT_SET_DUTY_DIRECTLY);
            iot_light_duty_write(light_dev_tmp->light_handle, IOT_CHANNEL_ID_B, 0, LIGHT_SET_DUTY_DIRECTLY);
            break;

        case LIGHT_CLOUD_DISCONNECTED:
            iot_light_breath_write(light_dev_tmp->light_handle, IOT_CHANNEL_ID_B, 4000);
            iot_light_duty_write(light_dev_tmp->light_handle, IOT_CHANNEL_ID_G, 0, LIGHT_SET_DUTY_DIRECTLY);
            iot_light_duty_write(light_dev_tmp->light_handle, IOT_CHANNEL_ID_R, 0, LIGHT_SET_DUTY_DIRECTLY);
            break;
            
        case LIGHT_CLOUD_CONNECTED:
            if (iot_param_load(IOT_LIGHT_NAME_SPACE, IOT_LIGHT_PARAM_KEY, &(light_dev_tmp->light_param)) != ESP_OK) {
                light_set(light_dev_tmp, 0, 0, 0, 2700, 0);
            } else {
                light_set(light_dev_tmp, light_dev_tmp->light_param.red, light_dev_tmp->light_param.green, 
                    light_dev_tmp->light_param.blue, light_dev_tmp->light_param.color_temp, light_dev_tmp->light_param.luminance); 
            }
            break;
            
        default:
            break;
    }   
    
    return ESP_OK;
}


