// Copyright 2015-2016 Espressif Systems (Shanghai) PTE LTD
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at

//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "freertos/timers.h"
#include "driver/touch_pad.h"
#include "driver/uart.h"
#include "esp_err.h"
#include "esp_wifi.h"
#include "nvs_flash.h"
#include "touch_tune_tool.h"
#include "sdkconfig.h"

#ifdef CONFIG_DATA_SCOPE_DEBUG

#define BUF_SIZE                                    (1024)
#define UART_PORT_NUM                               CONFIG_SCOPE_DEBUG_PORT_NUM
#define UART_BAUD_RATE                              CONFIG_SCOPE_DEBUG_BAUD_RATE
#define UART_RXD_PIN                                CONFIG_SCOPE_DEBUG_UART_RXD_IO
#define UART_TXD_PIN                                CONFIG_SCOPE_DEBUG_UART_TXD_IO
#define TOUCH_TUNING_TOOL_TASK_PRIORITY             CONFIG_SCOPE_DEBUG_TASK_PRIORITY

/* ESP-Tunint Tool static varible. */
static QueueHandle_t touch_tool_queue = NULL;
static uint8_t g_tune_mac[6];
static uint8_t s_dev_comb_num = 0;
static tune_dev_info_t s_dev_info = {0};
static tune_dev_setting_t s_dev_setting = {0};
static tune_dev_parameter_t s_dev_para = {0};
static tune_dev_data_t s_dev_data[TOUCH_PAD_MAX] = {0};

/* static function declaration. */
static void uart_init();
static void touch_tune_tool_task_create();
static void touch_tune_tool_read_task(void *pvParameter);
static void touch_tune_tool_write_task(void *pvParameter);
static esp_err_t read_frame(uint8_t *data);
static esp_err_t tune_tool_send_device_info(tune_dev_info_t *dev_info);
static esp_err_t tune_tool_send_device_setting(tune_dev_setting_t *dev_setting, uint8_t comb_num);
static esp_err_t tune_tool_send_device_parameter(tune_dev_parameter_t *dev_para);
static esp_err_t tune_tool_send_device_data(tune_dev_data_t *dev_data);
static tune_frame_all_t create_frame_from_str(uint8_t *data);
static uint8_t check_sum(tune_frame_all_t *frame);

void touch_tune_tool_init()
{
    uart_init();
    touch_tune_tool_task_create();
}

esp_err_t tune_tool_set_device_info(tune_dev_info_t *dev_info)
{
    if (dev_info != NULL) {
        memcpy(&s_dev_info, dev_info, sizeof(tune_dev_info_t));
        memcpy(g_tune_mac, s_dev_info.dev_mac, 6);
    }
    return ESP_OK;
}

esp_err_t tune_tool_add_device_setting(tune_dev_comb_t *ch_comb)
{
    if (ch_comb != NULL) {
        for (int i = 0; i < s_dev_comb_num; i++) {
            if (0 == memcmp(&s_dev_setting.dev_comb[s_dev_comb_num], ch_comb, sizeof(tune_dev_comb_t))) {
                return ESP_OK;
            }
        }
        memcpy(&(s_dev_setting.dev_comb[s_dev_comb_num]), ch_comb, sizeof(tune_dev_comb_t));
        for (int i = 0; i < ch_comb->ch_num_h * ch_comb->ch_num_l; i++) {
            s_dev_setting.ch_bits |= (1ULL << ch_comb->ch[i]);
        }
        s_dev_comb_num++;  //the combine num added.
    }
    return ESP_OK;
}

esp_err_t tune_tool_set_device_parameter(tune_dev_parameter_t *dev_para)
{
    if (dev_para != NULL) {
        memcpy(&s_dev_para, dev_para, sizeof(tune_dev_parameter_t));
    }
    return ESP_OK;
}

esp_err_t tune_tool_set_device_data(tune_dev_data_t *dev_data)
{
    if ((dev_data != NULL) && (dev_data->ch < TOUCH_PAD_MAX)) {
        memcpy(&s_dev_data[dev_data->ch], dev_data, sizeof(tune_dev_data_t));
    }
    return ESP_OK;
}

/**
 * @brief uart initialization
 */
static void uart_init()
{
    uart_config_t uart_config = {
        .baud_rate = UART_BAUD_RATE,
        .data_bits = UART_DATA_8_BITS,
        .parity    = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE
    };
    uart_param_config(UART_PORT_NUM, &uart_config);
    uart_set_pin(UART_PORT_NUM, UART_TXD_PIN, UART_RXD_PIN, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE);
    uart_driver_install(UART_PORT_NUM, 2 * 100, 2 * 100, 0, NULL, 0);
}

/**
 * @brief touch tune tool create read task and write task.
 */
static void touch_tune_tool_task_create()
{
    xTaskCreate(&touch_tune_tool_read_task, "touch_tune_tool_read_task", 2048, NULL, TOUCH_TUNING_TOOL_TASK_PRIORITY, NULL);
    xTaskCreate(&touch_tune_tool_write_task, "touch_tune_tool_write_task", 2048, NULL, TOUCH_TUNING_TOOL_TASK_PRIORITY, NULL);
}

/**
 * @brief touch tune tool read task.
 *
 * @param pvParameter the parameter of task.
 */
static void touch_tune_tool_read_task(void *pvParameter)
{
    // Configure a temporary buffer for the incoming data
    uint8_t *data = (uint8_t *) malloc(BUF_SIZE);
    char recv_mac[7] = {'\0'};
    touch_tool_evt_t evt;

    if (touch_tool_queue == NULL) {
        touch_tool_queue = xQueueCreate(10, sizeof(touch_tool_evt_t));
    }

    while (1) {
        if (read_frame(data) == ESP_OK) {
            tune_frame_all_t tune_frame = create_frame_from_str(data);
            if (tune_frame.check_sum == check_sum(&tune_frame)) {
                switch (tune_frame.frame.frame_type) {
                case TUNE_CTRL_DISCOVER:// send device info
                    evt.frame_type = TUNE_CTRL_DISCOVER;
                    xQueueSend(touch_tool_queue, &evt, portMAX_DELAY);
                    break;
                case TUNE_CTRL_INQUIRY:// send device setting, device parameter, device data
                    strncpy((char *)recv_mac, (char *)&tune_frame.frame.payload[1], 6);
                    if (strcmp((char *)recv_mac, (char *)g_tune_mac) == 0) {
                        switch (tune_frame.frame.payload[0]) {
                        case TUNE_DEV_SETTING:
                            evt.frame_type = TUNE_DEV_SETTING;
                            xQueueSend(touch_tool_queue, &evt, portMAX_DELAY);
                            break;
                        case TUNE_DEV_PARAMETER:
                            evt.frame_type = TUNE_DEV_PARAMETER;
                            xQueueSend(touch_tool_queue, &evt, portMAX_DELAY);
                            break;
                        case TUNE_DEV_DATA:
                            evt.frame_type = TUNE_DEV_DATA;
                            evt.time = xTaskGetTickCount();
                            xQueueSend(touch_tool_queue, &evt, portMAX_DELAY);
                            break;
                        default :
                            break;
                        }
                    }
                    break;
                case TUNE_CTRL_CANCEL:// no send
                    evt.frame_type = TUNE_CTRL_CANCEL;
                    xQueueSend(touch_tool_queue, &evt, portMAX_DELAY);
                    break;
                default:
                    break;
                }
            }
        }
    }
}

/**
 * @brief touch tune tool send task.
 *
 * @param pvParameter the parameter of task.
 */
static void touch_tune_tool_write_task(void *pvParameter)
{
    touch_tool_evt_t evt;
    static uint32_t time = 0;
    static bool send_enable = false;
    if (touch_tool_queue == NULL) {
        touch_tool_queue = xQueueCreate(10, sizeof(touch_tool_evt_t));
    }
    while (1) {
        if (xQueueReceive(touch_tool_queue, &evt, 20 / portTICK_RATE_MS) == pdTRUE) {
            switch (evt.frame_type) {
            case TUNE_CTRL_DISCOVER:
                tune_tool_send_device_info(&s_dev_info);
                break;
            case TUNE_DEV_SETTING:
                tune_tool_send_device_setting(&s_dev_setting, s_dev_comb_num);
                break;
            case TUNE_DEV_PARAMETER:
                tune_tool_send_device_parameter(&s_dev_para);
                break;
            case TUNE_DEV_DATA:
                time = evt.time;
                send_enable = true;
                break;
            case TUNE_CTRL_CANCEL:
                send_enable = false;
                break;
            default:
                break;
            }
        }
        if (((xTaskGetTickCount() - time) < 5000 / portTICK_RATE_MS) && send_enable) {
            tune_tool_send_device_data(s_dev_data);
        } else {
            send_enable = false;
        }
    }
}

/**
 * @brief Read a complete frame of tune_frame_all_t from uart.
 *
 * @param data a pointer of data from uart.
 *
 * @return
 *     - ESP_OK  Success
 *     - ESP_FAIL Invalid data
 */
static esp_err_t read_frame(uint8_t *data)
{
    int length = 0;
    int frame_length = 0;

HEAD:
    length = uart_read_bytes(UART_PORT_NUM, data, 1, portMAX_DELAY);
    if ((length == 1) && (data[0] == 0x55)) {
        length = uart_read_bytes(UART_PORT_NUM, data + 1, 1, 20 / portTICK_RATE_MS);
        if ((length == 1) && (data[1] == 0xAA)) {
            // length
            length = uart_read_bytes(UART_PORT_NUM, data + 2, 2, 20 / portTICK_RATE_MS);
            if (length == 2) {
                // data
                frame_length = data[2] | (data[3] << 8);
                length = uart_read_bytes(UART_PORT_NUM, data + 4, ntohs(frame_length), 20 / portTICK_RATE_MS);
                if (length == ntohs(frame_length)) {
                    // checksum
                    uart_read_bytes(UART_PORT_NUM, data + 4 + ntohs(frame_length), 1, 20 / portTICK_RATE_MS);
                    return ESP_OK;
                }
            }
        } else {
            goto HEAD;
        }
    } else {
        goto HEAD;
    }
    return ESP_FAIL;
}

/**
 * @brief Send device info.
 *
 * @param dev_info a pointer of tune_dev_info_t.
 *
 * @return
 *     - ESP_OK  send success
 *     - ESP_FAIL send fail
 */
static esp_err_t tune_tool_send_device_info(tune_dev_info_t *dev_info)
{
    tune_frame_all_t tune_frame;

    uint16_t temp = 0;
    for (int i = 0; i < 6; i++) {
        temp += dev_info->dev_mac[i];
    }
    if (!temp) {
        return ESP_FAIL;
    }
    tune_frame.head = htons(0x55AA);
    tune_frame.length = htons(1 + 8); // the size of tune_dev_info_t frame struct is 9 bytes
    tune_frame.frame.frame_type = TUNE_DEV_INFO;
    tune_frame.frame.payload[0] = dev_info->dev_cid;
    tune_frame.frame.payload[1] = dev_info->dev_ver;
    strncpy((char *)&tune_frame.frame.payload[2], (char *)dev_info->dev_mac, 6);
    tune_frame.check_sum = check_sum(&tune_frame);
    if (uart_write_bytes(UART_PORT_NUM, (char *)&tune_frame.head, 4) == -1) { //send head, length
        return ESP_FAIL;
    }
    if (uart_write_bytes(UART_PORT_NUM, (char *)&tune_frame.frame.frame_type, ntohs(tune_frame.length)) == -1) { // send frame_type, payload
        return ESP_FAIL;
    }
    if (uart_write_bytes(UART_PORT_NUM, (char *)&tune_frame.check_sum, 1) == -1) { // send check_sum
        return ESP_FAIL;
    }
    return ESP_OK;
}

/**
 * @brief Send device set.
 *
 * @param dev_setting a pointer of tune_dev_setting_t.
 * @param comb_num the sum of combine.
 *
 * @return
 *     - ESP_OK  send success
 *     - ESP_FAIL send fail
 */
static esp_err_t tune_tool_send_device_setting(tune_dev_setting_t *dev_setting, uint8_t comb_num)
{
    tune_frame_all_t tune_frame;
    uint16_t length = 0;

    if (!dev_setting->ch_bits) {
        return ESP_FAIL;
    }
    tune_frame.head = htons(0x55AA);
    // length
    tune_frame.frame.frame_type = TUNE_DEV_SETTING;
    uint32_convert u32_convert ;
    u32_convert.data = dev_setting->ch_bits;
    tune_frame.frame.payload[0] = u32_convert.byte[0];
    tune_frame.frame.payload[1] = u32_convert.byte[1];
    tune_frame.frame.payload[2] = u32_convert.byte[2];
    tune_frame.frame.payload[3] = u32_convert.byte[3];
    for (uint8_t i = 0; i < comb_num; i++) {
        tune_frame.frame.payload[4 + length] = dev_setting->dev_comb[i].dev_comb;
        tune_frame.frame.payload[5 + length] = dev_setting->dev_comb[i].ch_num_h;
        tune_frame.frame.payload[6 + length] = dev_setting->dev_comb[i].ch_num_l;
        if (dev_setting->dev_comb[i].dev_comb == TUNE_CHAR_MATRIX) {
            for (uint16_t j = 0; j < dev_setting->dev_comb[i].ch_num_h + dev_setting->dev_comb[i].ch_num_l; j++) {
                tune_frame.frame.payload[7 + length + j] = dev_setting->dev_comb[i].ch[j];
            }
            length += (dev_setting->dev_comb[i].ch_num_h + dev_setting->dev_comb[i].ch_num_l + 3);
        } else {
            for (uint16_t j = 0; j < dev_setting->dev_comb[i].ch_num_l; j++) {
                tune_frame.frame.payload[7 + length + j] = dev_setting->dev_comb[i].ch[j];
            }
            length += (dev_setting->dev_comb[i].ch_num_l + 3);
        }
    }
    length += 5;
    tune_frame.length = htons(length); // the size of tune_dev_data_t frame struct is x bytes
    tune_frame.check_sum = check_sum(&tune_frame);
    if (uart_write_bytes(UART_PORT_NUM, (char *)&tune_frame.head, 4) == -1) { //send head, length
        return ESP_FAIL;
    }
    if (uart_write_bytes(UART_PORT_NUM, (char *)&tune_frame.frame.frame_type, ntohs(tune_frame.length)) == -1) { // send frame_type, payload
        return ESP_FAIL;
    }
    if (uart_write_bytes(UART_PORT_NUM, (char *)&tune_frame.check_sum, 1) == -1) { // send check_sum
        return ESP_FAIL;
    }
    return ESP_OK;
}

/**
 * @brief Send device parameter.
 *
 * @param dev_para a pointer of tune_dev_parameter_t to send.
 *
 * @return
 *     - ESP_OK  send success
 *     - ESP_FAIL send fail
 */
static esp_err_t tune_tool_send_device_parameter(tune_dev_parameter_t *dev_para)
{
    uint8_t *temp_data_pointer = (uint8_t *)&dev_para->filter_period;
    tune_frame_all_t tune_frame;
    tune_frame.head = htons(0x55AA);
    tune_frame.length = htons(1 + sizeof(tune_dev_parameter_t)); // the size of tune_dev_parameter_t frame struct is 12 bytes
    tune_frame.frame.frame_type = TUNE_DEV_PARAMETER;
    for (uint8_t i = 0; i < sizeof(tune_dev_parameter_t); i++) {
        tune_frame.frame.payload[i] = temp_data_pointer[i];
    }
    tune_frame.check_sum = check_sum(&tune_frame);
    if (uart_write_bytes(UART_PORT_NUM, (char *)&tune_frame.head, 4) == -1) { //send head, length
        return ESP_FAIL;
    }
    if (uart_write_bytes(UART_PORT_NUM, (char *)&tune_frame.frame.frame_type, ntohs(tune_frame.length)) == -1) { // send frame_type, payload
        return ESP_FAIL;
    }
    if (uart_write_bytes(UART_PORT_NUM, (char *)&tune_frame.check_sum, 1) == -1) { // send check_sum
        return ESP_FAIL;
    }
    return ESP_OK;
}

/**
 * @brief Send device data.
 *
 * @param dev_data a pointer of tune_dev_data_t.
 *
 * @return
 *     - ESP_OK  send success
 *     - ESP_FAIL send fail
 */
static esp_err_t tune_tool_send_device_data(tune_dev_data_t *dev_data)
{
    uint8_t num = 0;
    uint8_t *temp_data_pointer = (uint8_t *)&dev_data[0].ch;
    tune_frame_all_t tune_frame;
    tune_frame.head = htons(0x55AA);
    tune_frame.frame.frame_type = TUNE_DEV_DATA;
    for (uint8_t i = 0; i < TOUCH_PAD_MAX; ++i) {
        if (s_dev_setting.ch_bits >> i & 0x01) {
            memcpy(&tune_frame.frame.payload[num++*sizeof(tune_dev_data_t)], &temp_data_pointer[i * sizeof(tune_dev_data_t)], sizeof(tune_dev_data_t));
        }
    }
    tune_frame.length = htons(1 + sizeof(tune_dev_data_t) * num); // the size of tune_dev_data_t frame struct is 9 bytes
    tune_frame.check_sum = check_sum(&tune_frame);
    if (uart_write_bytes(UART_PORT_NUM, (char *)&tune_frame.head, 4) == -1) { //send head, length
        return ESP_FAIL;
    }
    if (uart_write_bytes(UART_PORT_NUM, (char *)&tune_frame.frame.frame_type, ntohs(tune_frame.length)) == -1) { // send frame_type, payload
        return ESP_FAIL;
    }
    if (uart_write_bytes(UART_PORT_NUM, (char *)&tune_frame.check_sum, 1) == -1) { // send check_sum
        return ESP_FAIL;
    }
    return ESP_OK;
}

/**
 * @brief Create frame of tune_frame_all_t from data.
 *
 * @param data a pointer of passed data.
 *
 * @return a frame of tune_frame_all_t from data.
 */
static tune_frame_all_t create_frame_from_str(uint8_t *data)
{
    tune_frame_all_t tune_frame;
    tune_frame.head = data[0] | (data[1] << 8);
    tune_frame.length = data[2] | (data[3] << 8);
    tune_frame.frame.frame_type = data[4];
    for (uint16_t i = 0; i < ntohs(tune_frame.length); i++) {
        tune_frame.frame.payload[i] = data[5 + i];
    }
    tune_frame.check_sum = data[5 + ntohs(tune_frame.length) - 1];
    return tune_frame;
}

/**
 * @brief Check sum of frame of tune_frame_all_t.
 *
 * @param frame a pointer of passed frame of tune_frame_all_t to be checked.
 *
 * @return sum of frame.
 */
static uint8_t check_sum(tune_frame_all_t *frame)
{
    uint8_t check_sum = 0;
    uint8_t *pointer = (uint8_t *)(&frame->head);
    for (uint8_t i = 0; i < 5; i++) {
        check_sum += pointer[i];
    }
    if (ntohs(frame->length) != 0) {
        for (uint16_t i = 0; i < (ntohs(frame->length) - 1); i++) {
            check_sum += frame->frame.payload[i];
        }
    }
    return check_sum;
}

#endif // CONFIG_DATA_SCOPE_DEBUG
