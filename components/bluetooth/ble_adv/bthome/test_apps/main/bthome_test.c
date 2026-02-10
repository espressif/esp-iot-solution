/*
 * SPDX-FileCopyrightText: 2024-2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "esp_err.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "unity.h"
#include "bthome_v2.h"

static const char *TAG = "bthome_test";

#define TEST_MEMORY_LEAK_THRESHOLD (-460)

static size_t before_free_8bit;
static size_t before_free_32bit;

// Test data for various sensor types
static const float test_temperature = 23.5f;
static const float test_humidity = 65.2f;
static const float test_pressure = 1013.25f;
static const float test_illuminance = 500.0f;
static const uint32_t test_energy = 12345;
static const float test_power = 15.5f;
static const float test_voltage = 3.3f;
static const uint16_t test_pm25 = 25;
static const uint16_t test_pm10 = 35;
static const uint16_t test_co2 = 400;
static const uint16_t test_tvoc = 50;
static const uint8_t test_battery = 85;
static const uint8_t test_motion = 1;
static const uint8_t test_door = 0;

// Test encryption key
static const uint8_t test_key[16] = {
    0x23, 0x1d, 0x39, 0xc1, 0xd7, 0xcc, 0x1a, 0xb1, 0xae, 0xe2, 0x24, 0xcd, 0x09, 0x6d, 0xb9, 0x32
};

// Test MAC addresses
static const uint8_t test_local_mac[6] = {0xAA, 0xBB, 0xCC, 0xDD, 0xEE, 0xFF};
static const uint8_t test_peer_mac[6] = {0x54, 0x48, 0xE6, 0x8F, 0x80, 0xA5};

// Mock storage functions
static void mock_store_func(bthome_handle_t handle, const char *key, const uint8_t *data, uint8_t len)
{
    ESP_LOGI(TAG, "Mock store: key=%s, len=%d", key, len);
    // In real implementation, this would store data to NVS or flash
}

static void mock_load_func(bthome_handle_t handle, const char *key, uint8_t *data, uint8_t len)
{
    ESP_LOGI(TAG, "Mock load: key=%s, len=%d", key, len);
    // In real implementation, this would load data from NVS or flash
    // For testing, we'll just zero the data
    memset(data, 0, len);
}

static void check_leak(size_t before_free, size_t after_free, const char *type)
{
    ssize_t delta = after_free - before_free;
    printf("MALLOC_CAP_%s: Before %u bytes free, After %u bytes free (delta %d)\n", type, before_free, after_free, delta);
    TEST_ASSERT_MESSAGE(delta >= TEST_MEMORY_LEAK_THRESHOLD, "memory leak");
}

static void setup_test(void)
{
    before_free_8bit = heap_caps_get_free_size(MALLOC_CAP_8BIT);
    before_free_32bit = heap_caps_get_free_size(MALLOC_CAP_32BIT);
}

static void teardown_test(void)
{
    size_t after_free_8bit = heap_caps_get_free_size(MALLOC_CAP_8BIT);
    size_t after_free_32bit = heap_caps_get_free_size(MALLOC_CAP_32BIT);

    size_t leaked_8bit = before_free_8bit - after_free_8bit;
    size_t leaked_32bit = before_free_32bit - after_free_32bit;

    ESP_LOGI(TAG, "Memory leak check: 8bit=%d, 32bit=%d", (int)leaked_8bit, (int)leaked_32bit);

    check_leak(before_free_8bit, after_free_8bit, "8BIT");
    check_leak(before_free_32bit, after_free_32bit, "32BIT");
}

TEST_CASE("bthome_create_delete", "[bthome]")
{
    setup_test();

    bthome_handle_t handle = NULL;

    // Test create
    esp_err_t ret = bthome_create(&handle);
    TEST_ASSERT_EQUAL(ESP_OK, ret);
    TEST_ASSERT_NOT_NULL(handle);

    // Test delete
    ret = bthome_delete(handle);
    TEST_ASSERT_EQUAL(ESP_OK, ret);

    teardown_test();
}

TEST_CASE("bthome_encryption_config", "[bthome]")
{
    setup_test();

    bthome_handle_t handle = NULL;
    esp_err_t ret = bthome_create(&handle);
    TEST_ASSERT_EQUAL(ESP_OK, ret);

    // Test encryption key setting
    ret = bthome_set_encrypt_key(handle, test_key);
    TEST_ASSERT_EQUAL(ESP_OK, ret);

    // Test MAC address setting
    ret = bthome_set_local_mac_addr(handle, (uint8_t *)test_local_mac);
    TEST_ASSERT_EQUAL(ESP_OK, ret);

    ret = bthome_set_peer_mac_addr(handle, test_peer_mac);
    TEST_ASSERT_EQUAL(ESP_OK, ret);

    // Test callback registration
    bthome_callbacks_t callbacks = {
        .store = mock_store_func,
        .load = mock_load_func
    };
    ret = bthome_register_callbacks(handle, &callbacks);
    TEST_ASSERT_EQUAL(ESP_OK, ret);

    ret = bthome_delete(handle);
    TEST_ASSERT_EQUAL(ESP_OK, ret);

    teardown_test();
}

TEST_CASE("bthome_payload_creation", "[bthome]")
{
    setup_test();

    uint8_t buffer[100];
    uint8_t offset = 0;

    // Test temperature sensor data
    uint16_t temp_data = (uint16_t)(test_temperature * 100);
    offset = bthome_payload_add_sensor_data(buffer, offset, BTHOME_SENSOR_ID_TEMPERATURE_PRECISE,
                                            (uint8_t *)&temp_data, sizeof(temp_data));
    TEST_ASSERT_GREATER_THAN(0, offset);

    // Test humidity sensor data
    uint16_t hum_data = (uint16_t)(test_humidity * 100);
    offset = bthome_payload_add_sensor_data(buffer, offset, BTHOME_SENSOR_ID_HUMIDITY_PRECISE,
                                            (uint8_t *)&hum_data, sizeof(hum_data));
    TEST_ASSERT_GREATER_THAN(0, offset);

    // Test pressure sensor data
    uint16_t press_data = (uint16_t)(test_pressure * 100);
    offset = bthome_payload_add_sensor_data(buffer, offset, BTHOME_SENSOR_ID_PRESSURE,
                                            (uint8_t *)&press_data, sizeof(press_data));
    TEST_ASSERT_GREATER_THAN(0, offset);

    // Test illuminance sensor data
    uint32_t illum_data = (uint32_t)test_illuminance;
    offset = bthome_payload_add_sensor_data(buffer, offset, BTHOME_SENSOR_ID_ILLUMINANCE,
                                            (uint8_t *)&illum_data, sizeof(illum_data));
    TEST_ASSERT_GREATER_THAN(0, offset);

    // Test energy sensor data
    offset = bthome_payload_add_sensor_data(buffer, offset, BTHOME_SENSOR_ID_ENERGY,
                                            (uint8_t *)&test_energy, sizeof(test_energy));
    TEST_ASSERT_GREATER_THAN(0, offset);

    // Test power sensor data
    uint16_t power_data = (uint16_t)(test_power * 100);
    offset = bthome_payload_add_sensor_data(buffer, offset, BTHOME_SENSOR_ID_POWER,
                                            (uint8_t *)&power_data, sizeof(power_data));
    TEST_ASSERT_GREATER_THAN(0, offset);

    // Test voltage sensor data
    uint16_t voltage_data = (uint16_t)(test_voltage * 100);
    offset = bthome_payload_add_sensor_data(buffer, offset, BTHOME_SENSOR_ID_VOLTAGE,
                                            (uint8_t *)&voltage_data, sizeof(voltage_data));
    TEST_ASSERT_GREATER_THAN(0, offset);

    // Test PM2.5 sensor data
    offset = bthome_payload_add_sensor_data(buffer, offset, BTHOME_SENSOR_ID_PM25,
                                            (uint8_t *)&test_pm25, sizeof(test_pm25));
    TEST_ASSERT_GREATER_THAN(0, offset);

    // Test PM10 sensor data
    offset = bthome_payload_add_sensor_data(buffer, offset, BTHOME_SENSOR_ID_PM10,
                                            (uint8_t *)&test_pm10, sizeof(test_pm10));
    TEST_ASSERT_GREATER_THAN(0, offset);

    // Test CO2 sensor data
    offset = bthome_payload_add_sensor_data(buffer, offset, BTHOME_SENSOR_ID_CO2,
                                            (uint8_t *)&test_co2, sizeof(test_co2));
    TEST_ASSERT_GREATER_THAN(0, offset);

    // Test TVOC sensor data
    offset = bthome_payload_add_sensor_data(buffer, offset, BTHOME_SENSOR_ID_TVOC,
                                            (uint8_t *)&test_tvoc, sizeof(test_tvoc));
    TEST_ASSERT_GREATER_THAN(0, offset);

    // Test battery sensor data
    offset = bthome_payload_add_sensor_data(buffer, offset, BTHOME_SENSOR_ID_BATTERY,
                                            (uint8_t *)&test_battery, sizeof(test_battery));
    TEST_ASSERT_GREATER_THAN(0, offset);

    ESP_LOGI(TAG, "Total payload size: %d bytes", offset);

    teardown_test();
}

TEST_CASE("bthome_binary_sensor_data", "[bthome]")
{
    setup_test();

    uint8_t buffer[50];
    uint8_t offset = 0;

    // Test motion binary sensor
    offset = bthome_payload_adv_add_bin_sensor_data(buffer, offset, BTHOME_BIN_SENSOR_ID_MOTION, test_motion);
    TEST_ASSERT_GREATER_THAN(0, offset);

    // Test door binary sensor
    offset = bthome_payload_adv_add_bin_sensor_data(buffer, offset, BTHOME_BIN_SENSOR_ID_DOOR, test_door);
    TEST_ASSERT_GREATER_THAN(0, offset);

    // Test power binary sensor
    offset = bthome_payload_adv_add_bin_sensor_data(buffer, offset, BTHOME_BIN_SENSOR_ID_POWER, 1);
    TEST_ASSERT_GREATER_THAN(0, offset);

    // Test light binary sensor
    offset = bthome_payload_adv_add_bin_sensor_data(buffer, offset, BTHOME_BIN_SENSOR_ID_LIGHT, 0);
    TEST_ASSERT_GREATER_THAN(0, offset);

    ESP_LOGI(TAG, "Binary sensor payload size: %d bytes", offset);

    teardown_test();
}

TEST_CASE("bthome_event_data", "[bthome]")
{
    setup_test();

    uint8_t buffer[50];
    uint8_t offset = 0;

    // Test button event
    uint8_t button_event = 1; // Button pressed
    offset = bthome_payload_adv_add_evt_data(buffer, offset, BTHOME_EVENT_ID_BUTTON, &button_event, sizeof(button_event));
    TEST_ASSERT_GREATER_THAN(0, offset);

    // Test dimmer event
    uint8_t dimmer_event = 50; // Dimmer value
    offset = bthome_payload_adv_add_evt_data(buffer, offset, BTHOME_EVENT_ID_DIMMER, &dimmer_event, sizeof(dimmer_event));
    TEST_ASSERT_GREATER_THAN(0, offset);

    ESP_LOGI(TAG, "Event payload size: %d bytes", offset);

    teardown_test();
}

TEST_CASE("bthome_adv_data_creation", "[bthome]")
{
    setup_test();

    bthome_handle_t handle = NULL;
    esp_err_t ret = bthome_create(&handle);
    TEST_ASSERT_EQUAL(ESP_OK, ret);

    // Configure encryption
    ret = bthome_set_encrypt_key(handle, test_key);
    TEST_ASSERT_EQUAL(ESP_OK, ret);

    ret = bthome_set_local_mac_addr(handle, (uint8_t *)test_local_mac);
    TEST_ASSERT_EQUAL(ESP_OK, ret);

    // Register callbacks
    bthome_callbacks_t callbacks = {
        .store = mock_store_func,
        .load = mock_load_func
    };
    ret = bthome_register_callbacks(handle, &callbacks);
    TEST_ASSERT_EQUAL(ESP_OK, ret);

    // Create payload
    uint8_t payload[50];
    uint8_t payload_len = 0;

    // Add temperature data
    uint16_t temp_data = (uint16_t)(test_temperature * 100);
    payload_len = bthome_payload_add_sensor_data(payload, payload_len, BTHOME_SENSOR_ID_TEMPERATURE_PRECISE,
                                                 (uint8_t *)&temp_data, sizeof(temp_data));

    // Add humidity data
    uint16_t hum_data = (uint16_t)(test_humidity * 100);
    payload_len = bthome_payload_add_sensor_data(payload, payload_len, BTHOME_SENSOR_ID_HUMIDITY_PRECISE,
                                                 (uint8_t *)&hum_data, sizeof(hum_data));

    // Add motion binary sensor
    payload_len = bthome_payload_adv_add_bin_sensor_data(payload, payload_len, BTHOME_BIN_SENSOR_ID_MOTION, test_motion);

    // Create advertisement data
    uint8_t adv_data[100];
    const char *device_name = "ESP32-BTHome";
    uint8_t name_len = strlen(device_name);

    bthome_device_info_t device_info = {
        .bit = {
            .encryption_flag = 1,
            .trigger_based_flag = 0,
            .bthome_version = 2
        }
    };

    uint8_t adv_len = bthome_make_adv_data(handle, adv_data, (uint8_t *)device_name, name_len,
                                           device_info, payload, payload_len);

    ESP_LOGI(TAG, "Advertisement data length: %d bytes", adv_len);

    // Verify advertisement data structure
    TEST_ASSERT_EQUAL(0x02, adv_data[0]); // Length of flags
    TEST_ASSERT_EQUAL(0x01, adv_data[1]); // Flags type
    TEST_ASSERT_EQUAL(0x06, adv_data[2]); // Flags value

    ret = bthome_delete(handle);
    TEST_ASSERT_EQUAL(ESP_OK, ret);

    teardown_test();
}

TEST_CASE("bthome_adv_data_parsing", "[bthome]")
{
    setup_test();

    bthome_handle_t handle = NULL;
    esp_err_t ret = bthome_create(&handle);
    TEST_ASSERT_EQUAL(ESP_OK, ret);

    // Configure for decryption
    ret = bthome_set_encrypt_key(handle, test_key);
    TEST_ASSERT_EQUAL(ESP_OK, ret);

    ret = bthome_set_peer_mac_addr(handle, test_peer_mac);
    TEST_ASSERT_EQUAL(ESP_OK, ret);

    // Register callbacks
    bthome_callbacks_t callbacks = {
        .store = mock_store_func,
        .load = mock_load_func
    };
    ret = bthome_register_callbacks(handle, &callbacks);
    TEST_ASSERT_EQUAL(ESP_OK, ret);

    // Create test advertisement data (simplified, non-encrypted for testing)
    uint8_t test_adv_data[] = {
        0x02, 0x01, 0x06, 0x04, 0x09, 0x44, 0x49, 0x59, 0x11, 0x16, 0xd2, 0xfc, 0x41, 0xe6, 0x8b, 0x80, 0x0d, 0xd8, 0x00, 0x00, 0x00, 0x00, 0x7a, 0xcd, 0xcd, 0xfb
    };

    bthome_reports_t *reports = bthome_parse_adv_data(handle, test_adv_data, sizeof(test_adv_data));

    if (reports != NULL) {
        ESP_LOGI(TAG, "Parsed %d reports", reports->num_reports);

        // Verify we got some reports
        TEST_ASSERT_GREATER_THAN(0, reports->num_reports);
        TEST_ASSERT_LESS_OR_EQUAL(BTHOME_REPORTS_MAX, reports->num_reports);

        // Free the reports
        bthome_free_reports(reports);
    } else {
        ESP_LOGW(TAG, "Failed to parse advertisement data (expected for encrypted data)");
    }

    ret = bthome_delete(handle);
    TEST_ASSERT_EQUAL(ESP_OK, ret);

    teardown_test();
}

TEST_CASE("bthome_memory_management", "[bthome]")
{
    setup_test();

    bthome_handle_t handle = NULL;

    // Test multiple create/delete cycles
    for (int i = 0; i < 10; i++) {
        esp_err_t ret = bthome_create(&handle);
        TEST_ASSERT_EQUAL(ESP_OK, ret);
        TEST_ASSERT_NOT_NULL(handle);

        ret = bthome_delete(handle);
        TEST_ASSERT_EQUAL(ESP_OK, ret);
        handle = NULL;
    }

    teardown_test();
}

TEST_CASE("bthome_error_handling", "[bthome]")
{
    setup_test();

    bthome_handle_t handle = NULL;

    // Test create with NULL handle
    esp_err_t ret = bthome_create(NULL);
    TEST_ASSERT_NOT_EQUAL(ESP_OK, ret);

    // Test delete with NULL handle
    ret = bthome_delete(NULL);
    TEST_ASSERT_NOT_EQUAL(ESP_OK, ret);

    // Test operations with NULL handle
    ret = bthome_set_encrypt_key(NULL, test_key);
    TEST_ASSERT_NOT_EQUAL(ESP_OK, ret);

    ret = bthome_set_local_mac_addr(NULL, (uint8_t *)test_local_mac);
    TEST_ASSERT_NOT_EQUAL(ESP_OK, ret);

    ret = bthome_set_peer_mac_addr(NULL, test_peer_mac);
    TEST_ASSERT_NOT_EQUAL(ESP_OK, ret);

    bthome_callbacks_t callbacks = {
        .store = mock_store_func,
        .load = mock_load_func
    };
    ret = bthome_register_callbacks(NULL, &callbacks);
    TEST_ASSERT_NOT_EQUAL(ESP_OK, ret);

    ret = bthome_load_params(NULL);
    TEST_ASSERT_NOT_EQUAL(ESP_OK, ret);

    // Test with valid handle
    ret = bthome_create(&handle);
    TEST_ASSERT_EQUAL(ESP_OK, ret);

    // Test with NULL parameters
    ret = bthome_set_encrypt_key(handle, NULL);
    TEST_ASSERT_NOT_EQUAL(ESP_OK, ret);

    ret = bthome_set_local_mac_addr(handle, NULL);
    TEST_ASSERT_NOT_EQUAL(ESP_OK, ret);

    ret = bthome_set_peer_mac_addr(handle, NULL);
    TEST_ASSERT_NOT_EQUAL(ESP_OK, ret);

    ret = bthome_register_callbacks(handle, NULL);
    TEST_ASSERT_NOT_EQUAL(ESP_OK, ret);

    ret = bthome_delete(handle);
    TEST_ASSERT_EQUAL(ESP_OK, ret);

    teardown_test();
}

void app_main(void)
{
    printf("bthome_test\n");
    unity_run_menu();
}
