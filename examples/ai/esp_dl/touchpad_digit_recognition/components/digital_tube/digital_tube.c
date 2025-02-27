/*
 * SPDX-FileCopyrightText: 2021-2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "digital_tube.h"

extern const uint16_t index_lookup_table[];
extern const uint8_t code_lookup_table[];

esp_err_t digital_tube_write_num(uint8_t index, uint8_t num)
{
    if ((index >= CH455_MAX_TUBE_NUM) || (num > 9)) { //4 digital tube inside dev-board
        return ESP_ERR_INVALID_ARG;
    }
    esp_err_t ret = ch455g_write_cmd(index_lookup_table[index] | code_lookup_table[num]);
    if (ret != ESP_OK) {
        return ret;
    }
    return ESP_OK;
}

esp_err_t digital_tube_show_x(uint8_t num)
{
    esp_err_t ret;
    ret = digital_tube_write_num(0, num / 10);
    if (ret != ESP_OK) {
        return ret;
    }
    ret = digital_tube_write_num(3, num % 10);
    if (ret != ESP_OK) {
        return ret;
    }
    return ESP_OK;
}

esp_err_t digital_tube_show_y(uint8_t num)
{
    esp_err_t ret;
    ret = digital_tube_write_num(1, num / 10);
    if (ret != ESP_OK) {
        return ret;
    }
    ret = digital_tube_write_num(2, num % 10);
    if (ret != ESP_OK) {
        return ret;
    }
    return ESP_OK;
}

esp_err_t digital_tube_enable(void)
{
    esp_err_t ret = ch455g_write_cmd(CH455_SYS_ON);
    if (ret != ESP_OK) {
        return ret;
    }
    ret = ch455g_write_cmd(CH455_BIT_7SEG);
    if (ret != ESP_OK) {
        return ret;
    }
    return ESP_OK;
}

esp_err_t digital_tube_clear(uint8_t index)
{
    if (index >= CH455_MAX_TUBE_NUM) {
        return ESP_ERR_INVALID_ARG;
    }
    esp_err_t ret = ch455g_write_cmd(index_lookup_table[index] | 0x00);
    if (ret != ESP_OK) {
        return ret;
    }
    return ESP_OK;
}

esp_err_t digital_tube_clear_all(void)
{
    esp_err_t ret;
    ret = digital_tube_clear(0);
    if (ret != ESP_OK) {
        return ret;
    }
    ret = digital_tube_clear(1);
    if (ret != ESP_OK) {
        return ret;
    }
    ret = digital_tube_clear(2);
    if (ret != ESP_OK) {
        return ret;
    }
    ret = digital_tube_clear(3);
    if (ret != ESP_OK) {
        return ret;
    }
    return ESP_OK;
}

void digital_tube_show_reload(uint32_t ms)
{
    digital_tube_clear_all();
    vTaskDelay(pdMS_TO_TICKS(100));

    ch455g_write_cmd(index_lookup_table[0] | 0x01);
    vTaskDelay(pdMS_TO_TICKS(ms));
    uint16_t begin = 0x20 | 0x01;
    for (int i = 0; i < 2; ++i) {
        ch455g_write_cmd(index_lookup_table[0] | begin);
        uint16_t temp = begin;
        begin >>= 1;
        begin |= temp;
        vTaskDelay(pdMS_TO_TICKS(ms));
    }
    begin = 0x20;
    for (int i = 0; i < 3; ++i) {
        ch455g_write_cmd(index_lookup_table[1] | begin);
        uint16_t temp = begin;
        begin >>= 1;
        begin |= temp;
        vTaskDelay(pdMS_TO_TICKS(ms));
    }
    begin = 0x08;
    for (int i = 0; i < 3; ++i) {
        ch455g_write_cmd(index_lookup_table[2] | begin);
        uint16_t temp = begin;
        begin >>= 1;
        begin |= temp;
        vTaskDelay(pdMS_TO_TICKS(ms));
    }
    begin = 0x04;
    for (int i = 0; i < 3; ++i) {
        ch455g_write_cmd(index_lookup_table[3] | begin);
        uint16_t temp = begin;
        begin >>= 1;
        begin |= temp;
        vTaskDelay(pdMS_TO_TICKS(ms));
    }
}
