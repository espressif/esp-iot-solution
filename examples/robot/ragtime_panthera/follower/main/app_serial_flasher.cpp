/*
 * SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include "esp_log.h"
#include "app_serial_flasher.h"

static const char *TAG = "app_serial_flasher";

extern const uint8_t merged_binary_bin_start[] asm("_binary_merged_binary_bin_start");
extern const uint8_t merged_binary_bin_end[] asm("_binary_merged_binary_bin_end");

static const char *get_error_string(const esp_loader_error_t error)
{
    const char *mapping[ESP_LOADER_ERROR_INVALID_RESPONSE + 1] = {
        "NONE", "UNKNOWN", "TIMEOUT", "IMAGE SIZE",
        "INVALID MD5", "INVALID PARAMETER", "INVALID TARGET",
        "UNSUPPORTED CHIP", "UNSUPPORTED FUNCTION", "INVALID RESPONSE"
    };

    assert(error <= ESP_LOADER_ERROR_INVALID_RESPONSE);

    return mapping[error];
}

esp_err_t app_serial_flasher_init(const loader_esp32_config_t *config)
{
    if (loader_port_esp32_init(config) != ESP_LOADER_SUCCESS) {
        ESP_LOGE(TAG, "serial initialization failed.");
        return ESP_FAIL;
    }

    esp_loader_connect_args_t connect_config = ESP_LOADER_CONNECT_DEFAULT();
    esp_loader_error_t err = esp_loader_connect(&connect_config);
    if (err != ESP_LOADER_SUCCESS) {
        ESP_LOGE(TAG, "Cannot connect to target. Error: %s\n", get_error_string(err));

        if (err == ESP_LOADER_ERROR_TIMEOUT) {
            ESP_LOGE(TAG, "Check if the host and the target are properly connected.\n");
        } else if (err == ESP_LOADER_ERROR_INVALID_TARGET) {
            ESP_LOGE(TAG, "You could be using an unsupported chip, or chip revision.\n");
        } else if (err == ESP_LOADER_ERROR_INVALID_RESPONSE) {
            ESP_LOGE(TAG, "Try lowering the transmission rate or using shorter wires to connect the host and the target.\n");
        }

        return ESP_FAIL;
    }
    ESP_LOGI(TAG, "Connected to target");

    // Change transmission rate to 230400
    err = esp_loader_change_transmission_rate(230400);
    if (err  == ESP_LOADER_ERROR_UNSUPPORTED_FUNC) {
        ESP_LOGE(TAG, "ESP8266 does not support change transmission rate command.");
        return ESP_FAIL;
    } else if (err != ESP_LOADER_SUCCESS) {
        ESP_LOGE(TAG, "Unable to change transmission rate on target.");
        return ESP_FAIL;
    } else {
        err = loader_port_change_transmission_rate(230400);
        if (err != ESP_LOADER_SUCCESS) {
            ESP_LOGE(TAG, "Unable to change transmission rate.");
            return ESP_FAIL;
        }
        ESP_LOGI(TAG, "Transmission rate changed.");
    }
    ESP_LOGI(TAG, "Transmission rate changed to 230400");

    err = esp_loader_flash_erase_region(0, 0x1000);
    if (err != ESP_LOADER_SUCCESS) {
        ESP_LOGE(TAG, "Failed to erase flash region");
        return ESP_FAIL;
    }

    ESP_LOGI(TAG, "Flash erased successfully");

    uint8_t payload[1024 * 2];
    const uint8_t *bin_addr = merged_binary_bin_start;
    size_t size = merged_binary_bin_end - merged_binary_bin_start;
    err = esp_loader_flash_start(0x00, size, sizeof(payload));

    if (err != ESP_LOADER_SUCCESS) {
        ESP_LOGE(TAG, "Erasing flash failed with error: %s.\n", get_error_string(err));

        if (err == ESP_LOADER_ERROR_INVALID_PARAM) {
            ESP_LOGE(TAG, "If using Secure Download Mode, double check that the specified "
                     "target flash size is correct.\n");
        }
        return ESP_FAIL;
    }
    ESP_LOGI(TAG, "Start programming");

    size_t binary_size = size;
    size_t written = 0;

    while (size > 0) {
        size_t to_read = MIN(size, sizeof(payload));
        memcpy(payload, bin_addr, to_read);

        err = esp_loader_flash_write(payload, to_read);
        if (err != ESP_LOADER_SUCCESS) {
            ESP_LOGE(TAG, "Packet could not be written! Error %s.\n", get_error_string(err));
            return ESP_FAIL;
        }

        size -= to_read;
        bin_addr += to_read;
        written += to_read;

        int progress = (int)(((float)written / binary_size) * 100);
        printf("\rProgress: %d %%", progress);
    };

    ESP_LOGI(TAG, "Finished programming");

    esp_loader_reset_target();

    return ESP_OK;
}
