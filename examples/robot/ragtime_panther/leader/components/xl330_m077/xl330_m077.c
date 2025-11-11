/*
 * SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <sys/queue.h>
#include <stdlib.h>
#include <string.h>
#include "esp_log.h"
#include "esp_check.h"
#include "xl330_m077.h"

static const char *TAG = "XL330_M077";

// Buffer size definitions
#define SEND_PACKET_BUFFER_SIZE    32
#define RECEIVE_PACKET_BUFFER_SIZE 32
#define WRITE_PARAMS_BUFFER_SIZE   16

typedef struct xl330_m077_obj {
    uint8_t id;
    float position;
    SLIST_ENTRY(xl330_m077_obj) next;
} xl330_m077_obj_t;

SLIST_HEAD(xl330_m077_head, xl330_m077_obj);

typedef struct xl330_m077_bus {
    uart_port_t uart_num;
    uint8_t *send_packet_buffer;
    uint8_t *receive_packet_buffer;
    uint8_t *write_params_buffer;
    struct xl330_m077_head device_list;
} xl330_m077_bus_t;

unsigned short update_crc(unsigned short crc_accum, unsigned char *data_blk_ptr, unsigned short data_blk_size)
{
    unsigned short i, j;
    unsigned short crc_table[256] = {
        0x0000, 0x8005, 0x800F, 0x000A, 0x801B, 0x001E, 0x0014, 0x8011,
        0x8033, 0x0036, 0x003C, 0x8039, 0x0028, 0x802D, 0x8027, 0x0022,
        0x8063, 0x0066, 0x006C, 0x8069, 0x0078, 0x807D, 0x8077, 0x0072,
        0x0050, 0x8055, 0x805F, 0x005A, 0x804B, 0x004E, 0x0044, 0x8041,
        0x80C3, 0x00C6, 0x00CC, 0x80C9, 0x00D8, 0x80DD, 0x80D7, 0x00D2,
        0x00F0, 0x80F5, 0x80FF, 0x00FA, 0x80EB, 0x00EE, 0x00E4, 0x80E1,
        0x00A0, 0x80A5, 0x80AF, 0x00AA, 0x80BB, 0x00BE, 0x00B4, 0x80B1,
        0x8093, 0x0096, 0x009C, 0x8099, 0x0088, 0x808D, 0x8087, 0x0082,
        0x8183, 0x0186, 0x018C, 0x8189, 0x0198, 0x819D, 0x8197, 0x0192,
        0x01B0, 0x81B5, 0x81BF, 0x01BA, 0x81AB, 0x01AE, 0x01A4, 0x81A1,
        0x01E0, 0x81E5, 0x81EF, 0x01EA, 0x81FB, 0x01FE, 0x01F4, 0x81F1,
        0x81D3, 0x01D6, 0x01DC, 0x81D9, 0x01C8, 0x81CD, 0x81C7, 0x01C2,
        0x0140, 0x8145, 0x814F, 0x014A, 0x815B, 0x015E, 0x0154, 0x8151,
        0x8173, 0x0176, 0x017C, 0x8179, 0x0168, 0x816D, 0x8167, 0x0162,
        0x8123, 0x0126, 0x012C, 0x8129, 0x0138, 0x813D, 0x8137, 0x0132,
        0x0110, 0x8115, 0x811F, 0x011A, 0x810B, 0x010E, 0x0104, 0x8101,
        0x8303, 0x0306, 0x030C, 0x8309, 0x0318, 0x831D, 0x8317, 0x0312,
        0x0330, 0x8335, 0x833F, 0x033A, 0x832B, 0x032E, 0x0324, 0x8321,
        0x0360, 0x8365, 0x836F, 0x036A, 0x837B, 0x037E, 0x0374, 0x8371,
        0x8353, 0x0356, 0x035C, 0x8359, 0x0348, 0x834D, 0x8347, 0x0342,
        0x03C0, 0x83C5, 0x83CF, 0x03CA, 0x83DB, 0x03DE, 0x03D4, 0x83D1,
        0x83F3, 0x03F6, 0x03FC, 0x83F9, 0x03E8, 0x83ED, 0x83E7, 0x03E2,
        0x83A3, 0x03A6, 0x03AC, 0x83A9, 0x03B8, 0x83BD, 0x83B7, 0x03B2,
        0x0390, 0x8395, 0x839F, 0x039A, 0x838B, 0x038E, 0x0384, 0x8381,
        0x0280, 0x8285, 0x828F, 0x028A, 0x829B, 0x029E, 0x0294, 0x8291,
        0x82B3, 0x02B6, 0x02BC, 0x82B9, 0x02A8, 0x82AD, 0x82A7, 0x02A2,
        0x82E3, 0x02E6, 0x02EC, 0x82E9, 0x02F8, 0x82FD, 0x82F7, 0x02F2,
        0x02D0, 0x82D5, 0x82DF, 0x02DA, 0x82CB, 0x02CE, 0x02C4, 0x82C1,
        0x8243, 0x0246, 0x024C, 0x8249, 0x0258, 0x825D, 0x8257, 0x0252,
        0x0270, 0x8275, 0x827F, 0x027A, 0x826B, 0x026E, 0x0264, 0x8261,
        0x0220, 0x8225, 0x822F, 0x022A, 0x823B, 0x023E, 0x0234, 0x8231,
        0x8213, 0x0216, 0x021C, 0x8219, 0x0208, 0x820D, 0x8207, 0x0202
    };

    for (j = 0; j < data_blk_size; j++) {
        i = ((unsigned short)(crc_accum >> 8) ^ data_blk_ptr[j]) & 0xFF;
        crc_accum = (crc_accum << 8) ^ crc_table[i];
    }

    return crc_accum;
}

static int xl330_m077_send_packet(xl330_m077_bus_t *bus, uint8_t id, uint8_t instruction, const uint8_t *params, uint8_t param_length)
{
    // Packet format: FF FF FD 00 ID Length_L Length_H Instruction [Param1 ... ParamN] CRC_L CRC_H
    // Length = Instruction(1) + Parameters(N) + CRC(2)
    uint8_t *packet = bus->send_packet_buffer;
    uint8_t pos = 0;

    // Header
    packet[pos++] = 0xFF;
    packet[pos++] = 0xFF;
    packet[pos++] = 0xFD;
    packet[pos++] = 0x00;

    // ID
    packet[pos++] = id;

    // Length (will be filled after calculating CRC)
    uint8_t length_pos = pos;
    pos += 2;

    // Instruction
    packet[pos++] = instruction;

    // Parameters
    if (params != NULL && param_length > 0) {
        for (uint8_t i = 0; i < param_length; i++) {
            packet[pos++] = params[i];
        }
    }

    // Calculate length: Instruction(1) + Parameters(N) + CRC(2)
    uint16_t length = 1 + param_length + 2;
    packet[length_pos] = length & 0xFF;
    packet[length_pos + 1] = (length >> 8) & 0xFF;

    // Calculate CRC from 0xFF 0xFF 0xFD 0x00 (packet[0]) to before CRC (packet[pos-1])
    // CRC covers: Header(4) + ID(1) + Length(2) + Instruction(1) + Parameters(N)
    uint16_t crc = update_crc(0, packet, pos);
    packet[pos++] = crc & 0xFF;
    packet[pos++] = (crc >> 8) & 0xFF;

    return uart_write_bytes(bus->uart_num, packet, pos);
}

static esp_err_t xl330_m077_receive_response(xl330_m077_bus_t *bus, uint8_t expected_id, uint32_t timeout_ms, uint8_t **params, uint8_t *param_length)
{
    uint8_t *buffer = bus->receive_packet_buffer;
    int len = uart_read_bytes(bus->uart_num, buffer, RECEIVE_PACKET_BUFFER_SIZE - 1, timeout_ms / portTICK_PERIOD_MS);
    if (len <= 0) {
        return ESP_ERR_TIMEOUT;
    }

    if (len < 11) {
        return ESP_ERR_INVALID_SIZE;
    }

    // Step 1: Check packet header first
    if (buffer[0] != 0xFF || buffer[1] != 0xFF || buffer[2] != 0xFD || buffer[3] != 0x00) {
        return ESP_ERR_INVALID_RESPONSE;
    }

    // Step 2: Read packet_length and calculate expected total length
    // Packet format: FF FF FD 00 ID Length_L Length_H Instruction Error_Code Parameter1 ... ParameterN CRC_L CRC_H
    // packet_length field (buffer[5-6], little endian) represents: Instruction + Error_Code + Parameters + CRC
    // So packet_length = 1 (Instruction) + 1 (Error_Code) + N (Parameters) + 2 (CRC)
    uint16_t packet_length = buffer[5] | (buffer[6] << 8);
    // Expected total length = Header(4) + ID(1) + Length_field(2) + packet_length
    // packet_length already includes Instruction, Error_Code, Parameters and CRC
    uint16_t expected_len = 4 + 1 + 2 + packet_length;

    if (len < expected_len) {
        return ESP_ERR_INVALID_SIZE;
    }

    if (packet_length < 4) {
        return ESP_ERR_INVALID_RESPONSE;
    }

    // Step 3: Verify CRC - if CRC is wrong, no need to parse other fields
    uint16_t crc = buffer[len - 1] << 8 | buffer[len - 2];
    uint16_t calculated_crc = update_crc(0, buffer, len - 2);
    if (crc != calculated_crc) {
        ESP_LOGE(TAG, "CRC mismatch: received 0x%04X, calculated 0x%04X", crc, calculated_crc);
        return ESP_ERR_INVALID_CRC;
    }

    // Step 4: Parse other fields after CRC verification
    uint8_t response_id = buffer[4];
    if (response_id != expected_id) {
        ESP_LOGE(TAG, "ID mismatch: expected %d, got %d", expected_id, response_id);
        return ESP_ERR_INVALID_RESPONSE;
    }

    uint8_t instruction = buffer[7];
    if (instruction != 0x55) {
        ESP_LOGE(TAG, "Invalid instruction: 0x%02X", instruction);
        return ESP_ERR_INVALID_RESPONSE;
    }

    uint8_t error_code = buffer[8];
    if (error_code != 0) {
        ESP_LOGW(TAG, "Error code: 0x%02X", error_code);
        return ESP_ERR_INVALID_RESPONSE;
    }

    // Extract parameters
    *param_length = packet_length - 4;
    *params = &buffer[9];

    return ESP_OK;
}

esp_err_t xl330_m077_ping(xl330_m077_bus_handle_t bus_handle, uint8_t id)
{
    if (bus_handle == NULL) {
        ESP_LOGE(TAG, "Bus handle is NULL");
        return ESP_ERR_INVALID_ARG;
    }

    xl330_m077_bus_t *bus = (xl330_m077_bus_t *)bus_handle;

    // PING instruction (0x01) with no parameters
    if (xl330_m077_send_packet(bus, id, 0x01, NULL, 0) < 0) {
        ESP_LOGE(TAG, "Failed to send ping command");
        return ESP_FAIL;
    }

    uint8_t *params;
    uint8_t param_length;
    esp_err_t ret = xl330_m077_receive_response(bus, id, 100, &params, &param_length);
    return ret;
}

static int xl330_m077_read(xl330_m077_bus_t *bus, uint8_t id, uint8_t address, uint8_t length)
{
    // READ instruction (0x02) with parameters: address (2 bytes) + length (2 bytes)
    uint8_t params[4] = {
        address & 0xFF,
        (address >> 8) & 0xFF,
        length & 0xFF,
        (length >> 8) & 0xFF
    };
    return xl330_m077_send_packet(bus, id, 0x02, params, 4);
}

static int xl330_m077_write(xl330_m077_bus_t *bus, uint8_t id, uint16_t address, uint8_t length, const uint8_t *data)
{
    // WRITE instruction (0x03) with parameters: address (2 bytes) + data (N bytes)
    // Use dedicated write_params_buffer to avoid conflict with send_packet_buffer
    uint8_t *params = bus->write_params_buffer;
    params[0] = address & 0xFF;
    params[1] = (address >> 8) & 0xFF;
    for (uint8_t i = 0; i < length; i++) {
        params[2 + i] = data[i];
    }
    return xl330_m077_send_packet(bus, id, 0x03, params, 2 + length);
}

xl330_m077_bus_handle_t xl330_m077_bus_init(const xl330_m077_bus_config_t *config)
{
    if (config == NULL) {
        ESP_LOGE(TAG, "Config pointer is NULL");
        return NULL;
    }

    if (config->uart_num >= UART_NUM_MAX) {
        ESP_LOGE(TAG, "Invalid UART number");
        return NULL;
    }

    if (!GPIO_IS_VALID_GPIO(config->tx_pin)) {
        ESP_LOGE(TAG, "Invalid TX pin");
        return NULL;
    }

    if (!GPIO_IS_VALID_GPIO(config->rx_pin)) {
        ESP_LOGE(TAG, "Invalid RX pin");
        return NULL;
    }

    xl330_m077_bus_t *bus = (xl330_m077_bus_t *)calloc(1, sizeof(xl330_m077_bus_t));
    if (bus == NULL) {
        ESP_LOGE(TAG, "Failed to allocate bus object");
        return NULL;
    }

    // Allocate packet buffers on heap
    bus->send_packet_buffer = (uint8_t *)calloc(1, SEND_PACKET_BUFFER_SIZE);
    if (bus->send_packet_buffer == NULL) {
        ESP_LOGE(TAG, "Failed to allocate send packet buffer");
        free(bus);
        return NULL;
    }

    bus->receive_packet_buffer = (uint8_t *)calloc(1, RECEIVE_PACKET_BUFFER_SIZE);
    if (bus->receive_packet_buffer == NULL) {
        ESP_LOGE(TAG, "Failed to allocate receive packet buffer");
        free(bus->send_packet_buffer);
        free(bus);
        return NULL;
    }

    bus->write_params_buffer = (uint8_t *)calloc(1, WRITE_PARAMS_BUFFER_SIZE);
    if (bus->write_params_buffer == NULL) {
        ESP_LOGE(TAG, "Failed to allocate write params buffer");
        free(bus->send_packet_buffer);
        free(bus->receive_packet_buffer);
        free(bus);
        return NULL;
    }

    uart_config_t uart_config = {
        .baud_rate = config->baud_rate,
        .data_bits = UART_DATA_8_BITS,
        .parity    = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
        .source_clk = UART_SCLK_DEFAULT,
    };

    if (ESP_OK != uart_driver_install(config->uart_num, 1024 * 2, 0, 0, NULL, 0)) {
        ESP_LOGE(TAG, "Failed to install UART driver");
        free(bus->send_packet_buffer);
        free(bus->receive_packet_buffer);
        free(bus->write_params_buffer);
        free(bus);
        return NULL;
    }

    if (ESP_OK != uart_param_config(config->uart_num, &uart_config)) {
        ESP_LOGE(TAG, "Failed to configure UART");
        uart_driver_delete(config->uart_num);
        free(bus->send_packet_buffer);
        free(bus->receive_packet_buffer);
        free(bus->write_params_buffer);
        free(bus);
        return NULL;
    }

    if (ESP_OK != uart_set_pin(config->uart_num, config->tx_pin, config->rx_pin, -1, -1)) {
        ESP_LOGE(TAG, "Failed to set UART pins");
        uart_driver_delete(config->uart_num);
        free(bus->send_packet_buffer);
        free(bus->receive_packet_buffer);
        free(bus->write_params_buffer);
        free(bus);
        return NULL;
    }

    bus->uart_num = config->uart_num;
    SLIST_INIT(&bus->device_list);

    return (xl330_m077_bus_handle_t)bus;
}

esp_err_t xl330_m077_scan_devices(xl330_m077_bus_handle_t bus_handle)
{
    if (bus_handle == NULL) {
        ESP_LOGE(TAG, "Bus handle is NULL");
        return ESP_ERR_INVALID_ARG;
    }

    uint8_t found_count = 0;
    uint8_t found_ids[253];
    uint8_t found_idx = 0;

    ESP_LOGI(TAG, "Starting device scan...");

    for (uint8_t id = 0; id <= 252; id++) {
        esp_err_t ret = xl330_m077_ping(bus_handle, id);
        if (ret == ESP_OK) {
            ESP_LOGI(TAG, "Found device with ID %d", id);
            found_ids[found_idx++] = id;
            found_count++;
        }
        vTaskDelay(10 / portTICK_PERIOD_MS);
    }

    ESP_LOGI(TAG, "Scan complete, found %d device(s):", found_count);
    for (uint8_t i = 0; i < found_count; i++) {
        ESP_LOGI(TAG, "  - ID %d", found_ids[i]);
    }

    return ESP_OK;
}

esp_err_t xl330_m077_add_device(xl330_m077_bus_handle_t bus_handle, uint8_t id)
{
    if (bus_handle == NULL) {
        ESP_LOGE(TAG, "Bus handle is NULL");
        return ESP_ERR_INVALID_ARG;
    }

    xl330_m077_bus_t *bus = (xl330_m077_bus_t *)bus_handle;
    ESP_RETURN_ON_FALSE(id <= 252, ESP_ERR_INVALID_ARG, TAG, "Invalid servo ID (must be 0-252)");

    xl330_m077_obj_t *obj;
    SLIST_FOREACH(obj, &bus->device_list, next) {
        if (obj->id == id) {
            ESP_LOGI(TAG, "Device ID %d already exists", id);
            return ESP_OK;
        }
    }

    obj = (xl330_m077_obj_t *)calloc(1, sizeof(xl330_m077_obj_t));
    if (obj == NULL) {
        ESP_LOGE(TAG, "Failed to allocate device object");
        return ESP_ERR_NO_MEM;
    }

    obj->id = id;
    SLIST_INSERT_HEAD(&bus->device_list, obj, next);

    ESP_LOGI(TAG, "Added servo device: ID=%d, UART=%d", id, bus->uart_num);
    return ESP_OK;
}

esp_err_t xl330_m077_read_position(xl330_m077_bus_handle_t bus_handle, uint8_t id, float *position)
{
    if (bus_handle == NULL) {
        ESP_LOGE(TAG, "Bus handle is NULL");
        return ESP_ERR_INVALID_ARG;
    }

    if (position == NULL) {
        ESP_LOGE(TAG, "Position pointer is NULL");
        return ESP_ERR_INVALID_ARG;
    }

    xl330_m077_bus_t *bus = (xl330_m077_bus_t *)bus_handle;
    xl330_m077_obj_t *obj;
    bool device_found = false;
    SLIST_FOREACH(obj, &bus->device_list, next) {
        if (obj->id == id) {
            device_found = true;
            break;
        }
    }

    if (!device_found) {
        ESP_LOGE(TAG, "Device ID %d not found", id);
        return ESP_ERR_NOT_FOUND;
    }

    if (xl330_m077_read(bus, id, 132, 4) < 0) {
        ESP_LOGE(TAG, "Failed to send read command");
        return ESP_FAIL;
    }

    uint8_t *params;
    uint8_t param_length;
    esp_err_t ret = xl330_m077_receive_response(bus, id, 10, &params, &param_length);
    if (ret != ESP_OK) {
        return ret;
    }

    if (param_length < 4) {
        ESP_LOGE(TAG, "Invalid parameter length: %d", param_length);
        return ESP_ERR_INVALID_SIZE;
    }

    int32_t raw_value = params[0] | (params[1] << 8) | (params[2] << 16) | (params[3] << 24);
    *position = ((float)raw_value * 1.0f / 4095.0f) * 360.0f;

    return ESP_OK;
}

esp_err_t xl330_m077_set_operation(xl330_m077_bus_handle_t bus_handle, uint8_t id, xl330_m077_operation_t operation)
{
    if (bus_handle == NULL) {
        ESP_LOGE(TAG, "Bus handle is NULL");
        return ESP_ERR_INVALID_ARG;
    }

    xl330_m077_bus_t *bus = (xl330_m077_bus_t *)bus_handle;
    xl330_m077_obj_t *obj;
    bool device_found = false;
    SLIST_FOREACH(obj, &bus->device_list, next) {
        if (obj->id == id) {
            device_found = true;
            break;
        }
    }

    if (!device_found) {
        ESP_LOGE(TAG, "Device ID %d not found", id);
        return ESP_ERR_NOT_FOUND;
    }

    uint8_t data = (uint8_t)operation;
    if (xl330_m077_write(bus, id, 0x0B, 1, &data) < 0) {
        ESP_LOGE(TAG, "Failed to send write command");
        return ESP_FAIL;
    }

    uint8_t *params;
    uint8_t param_length;
    esp_err_t ret = xl330_m077_receive_response(bus, id, 100, &params, &param_length);
    if (ret == ESP_OK) {
        ESP_LOGI(TAG, "Set operation mode %d for ID %d", operation, id);
    }
    return ret;
}

esp_err_t xl330_m077_control_led(xl330_m077_bus_handle_t bus_handle, uint8_t id, bool if_on)
{
    if (bus_handle == NULL) {
        ESP_LOGE(TAG, "Bus handle is NULL");
        return ESP_ERR_INVALID_ARG;
    }

    xl330_m077_bus_t *bus = (xl330_m077_bus_t *)bus_handle;
    xl330_m077_obj_t *obj;
    bool device_found = false;
    SLIST_FOREACH(obj, &bus->device_list, next) {
        if (obj->id == id) {
            device_found = true;
            break;
        }
    }

    if (!device_found) {
        ESP_LOGE(TAG, "Device ID %d not found", id);
        return ESP_ERR_NOT_FOUND;
    }

    uint8_t data = if_on ? 1 : 0;
    if (xl330_m077_write(bus, id, 0x41, 1, &data) < 0) {
        ESP_LOGE(TAG, "Failed to send write command");
        return ESP_FAIL;
    }

    uint8_t *params;
    uint8_t param_length;
    esp_err_t ret = xl330_m077_receive_response(bus, id, 100, &params, &param_length);
    if (ret == ESP_OK) {
        ESP_LOGI(TAG, "Control LED %s for ID %d", if_on ? "ON" : "OFF", id);
    }
    return ret;
}

esp_err_t xl330_m077_control_torque(xl330_m077_bus_handle_t bus_handle, uint8_t id, bool if_on)
{
    if (bus_handle == NULL) {
        ESP_LOGE(TAG, "Bus handle is NULL");
        return ESP_ERR_INVALID_ARG;
    }

    xl330_m077_bus_t *bus = (xl330_m077_bus_t *)bus_handle;
    xl330_m077_obj_t *obj;
    bool device_found = false;
    SLIST_FOREACH(obj, &bus->device_list, next) {
        if (obj->id == id) {
            device_found = true;
            break;
        }
    }

    if (!device_found) {
        ESP_LOGE(TAG, "Device ID %d not found", id);
        return ESP_ERR_NOT_FOUND;
    }

    uint8_t data = if_on ? 1 : 0;
    if (xl330_m077_write(bus, id, 0x40, 1, &data) < 0) {
        ESP_LOGE(TAG, "Failed to send write command");
        return ESP_FAIL;
    }

    uint8_t *params;
    uint8_t param_length;
    esp_err_t ret = xl330_m077_receive_response(bus, id, 100, &params, &param_length);
    if (ret == ESP_OK) {
        ESP_LOGI(TAG, "Control torque %s for ID %d", if_on ? "ON" : "OFF", id);
    }
    return ret;
}

esp_err_t xl330_m077_move_to_position(xl330_m077_bus_handle_t bus_handle, uint8_t id, float position)
{
    if (bus_handle == NULL) {
        ESP_LOGE(TAG, "Bus handle is NULL");
        return ESP_ERR_INVALID_ARG;
    }

    xl330_m077_bus_t *bus = (xl330_m077_bus_t *)bus_handle;
    xl330_m077_obj_t *obj;
    bool device_found = false;
    SLIST_FOREACH(obj, &bus->device_list, next) {
        if (obj->id == id) {
            device_found = true;
            break;
        }
    }

    if (!device_found) {
        ESP_LOGE(TAG, "Device ID %d not found", id);
        return ESP_ERR_NOT_FOUND;
    }

    uint32_t position_value = (uint32_t)(position * 4095.0f / 360.0f);
    uint8_t data[4] = {
        (uint8_t)(position_value & 0xFF),
        (uint8_t)((position_value >> 8) & 0xFF),
        (uint8_t)((position_value >> 16) & 0xFF),
        (uint8_t)((position_value >> 24) & 0xFF),
    };

    if (xl330_m077_write(bus, id, 0x74, 4, data) < 0) {
        ESP_LOGE(TAG, "Failed to send write command");
        return ESP_FAIL;
    }

    uint8_t *params;
    uint8_t param_length;
    esp_err_t ret = xl330_m077_receive_response(bus, id, 100, &params, &param_length);
    if (ret == ESP_OK) {
        ESP_LOGI(TAG, "Move to position %f for ID %d", position, id);
    }
    return ret;
}

esp_err_t xl330_m077_reboot(xl330_m077_bus_handle_t bus_handle, uint8_t id)
{

    if (bus_handle == NULL) {
        ESP_LOGE(TAG, "Bus handle is NULL");
        return ESP_ERR_INVALID_ARG;
    }

    xl330_m077_bus_t *bus = (xl330_m077_bus_t *)bus_handle;

    xl330_m077_obj_t *obj;
    bool device_found = false;
    SLIST_FOREACH(obj, &bus->device_list, next) {
        if (obj->id == id) {
            device_found = true;
            break;
        }
    }

    if (!device_found) {
        ESP_LOGE(TAG, "Device ID %d not found", id);
        return ESP_ERR_NOT_FOUND;
    }

    return xl330_m077_send_packet(bus, id, 0x08, NULL, 0);
}
