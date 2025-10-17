/*
 * SPDX-FileCopyrightText: 2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include <stdio.h>
#include <string.h>
#include "ble_hci.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "unity.h"

#define TEST_MEMORY_LEAK_THRESHOLD (-460)

static size_t before_free_8bit;
static size_t before_free_32bit;

static void ble_hci_scan_cb(ble_hci_scan_result_t *scan_result, uint16_t result_len)
{
    for (int i = 0; i < result_len; i++) {
        printf("%2x:%2x:%2x:%2x:%2x:%2x\n",
               scan_result[i].bda[0], scan_result[i].bda[1], scan_result[i].bda[2],
               scan_result[i].bda[3], scan_result[i].bda[4], scan_result[i].bda[5]);
    }
}

TEST_CASE("ble_hci_test", "[ble hci adv]")
{
    ble_hci_init();

    uint8_t own_addr[6] = {0xff, 0x22, 0x33, 0x44, 0x55, 0x66};
    ble_hci_set_random_address(own_addr);

    ble_hci_adv_param_t adv_param = {
        .adv_int_min = 0x20,
        .adv_int_max = 0x40,
        .adv_type = ADV_TYPE_NONCONN_IND,
        .own_addr_type = BLE_ADDR_TYPE_RANDOM,
        .peer_addr_type = BLE_ADDR_TYPE_PUBLIC,
        .channel_map = ADV_CHNL_ALL,
        .adv_filter_policy = ADV_FILTER_ALLOW_SCAN_ANY_CON_ANY,
    };
    uint8_t peer_addr[6] = {0x80, 0x81, 0x82, 0x83, 0x84, 0x85};
    memcpy(adv_param.peer_addr, peer_addr, BLE_HCI_ADDR_LEN);
    ble_hci_set_adv_param(&adv_param);

    char *adv_name = "ESP-BLE-1";
    uint8_t name_len = (uint8_t)strlen(adv_name);
    uint8_t adv_data[31] = {
        0x02, 0x01, 0x06, 0x0, 0x09
    };
    uint8_t adv_data_len;
    adv_data[3] = name_len + 1;
    memcpy(adv_data + 5, adv_name, name_len);
    adv_data_len = 5 + name_len;
    ble_hci_set_adv_data(adv_data_len, adv_data);

    ble_hci_set_adv_enable(true);

    vTaskDelay(5000 / portTICK_PERIOD_MS);

    ble_hci_set_adv_enable(false);
    ble_hci_deinit();
}

TEST_CASE("ble_hci_test", "[ble hci scan]")
{
    ble_hci_init();

    ble_hci_reset();
    ble_hci_enable_meta_event();

    ble_hci_scan_param_t scan_param = {
        .scan_type = BLE_SCAN_TYPE_PASSIVE,
        .scan_interval = 0x50,
        .scan_window = 0x50,
        .own_addr_type = BLE_ADDR_TYPE_PUBLIC,
        .filter_policy = ADV_FILTER_ALLOW_SCAN_WLST_CON_ANY,
    };

    ble_hci_set_scan_param(&scan_param);
    ble_hci_set_register_scan_callback(&ble_hci_scan_cb);
    uint8_t peer_addr[6] = {0xff, 0x22, 0x33, 0x44, 0x55, 0x66};
    ble_hci_add_to_accept_list(peer_addr, BLE_ADDR_TYPE_RANDOM);
    ble_hci_set_scan_enable(true, false);

    vTaskDelay(5000 / portTICK_PERIOD_MS);

    ble_hci_set_scan_enable(false, false);
    ble_hci_deinit();
}

static void check_leak(size_t before_free, size_t after_free, const char *type)
{
    ssize_t delta = after_free - before_free;
    printf("MALLOC_CAP_%s: Before %u bytes free, After %u bytes free (delta %d)\n", type, before_free, after_free, delta);
    TEST_ASSERT_MESSAGE(delta >= TEST_MEMORY_LEAK_THRESHOLD, "memory leak");
}

void setUp(void)
{
    before_free_8bit = heap_caps_get_free_size(MALLOC_CAP_8BIT);
    before_free_32bit = heap_caps_get_free_size(MALLOC_CAP_32BIT);
}

void tearDown(void)
{
    size_t after_free_8bit = heap_caps_get_free_size(MALLOC_CAP_8BIT);
    size_t after_free_32bit = heap_caps_get_free_size(MALLOC_CAP_32BIT);
    check_leak(before_free_8bit, after_free_8bit, "8BIT");
    check_leak(before_free_32bit, after_free_32bit, "32BIT");
}

void app_main(void)
{
    printf("ble_hci_adv_test\n");
    unity_run_menu();
}
