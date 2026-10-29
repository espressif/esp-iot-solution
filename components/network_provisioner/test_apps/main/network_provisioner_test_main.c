/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "unity.h"
#include "unity_test_runner.h"
#include "unity_test_utils_memory.h"

#include "esp_err.h"
#include "esp_http_client.h"

#include "network_config.pb-c.h"
#include "network_provisioner.h"
#include "network_provisioner_priv.h"
#include "protocomm_ext.h"
#include "protocomm_ext_security.h"

#define LEAKS_THRESHOLD_BYTES (400)

TEST_CASE("network_provisioner_create: NULL pc returns NULL", "[network_provisioner]")
{
    TEST_ASSERT_NULL(network_provisioner_create(NULL));
}

TEST_CASE("capabilities: parse wifi and thread flags from cap array", "[network_provisioner]")
{
    network_provisioner_capabilities_t caps = {0};
    const char *json =
        "{\"prov\":{\"ver\":\"v1.1\",\"sec_ver\":1,\"cap\":[\"wifi_scan\",\"wifi_prov\",\"no_pop\"]}}";
    TEST_ASSERT_EQUAL(ESP_OK, network_provisioner_parse_capabilities(json, &caps));
    TEST_ASSERT_TRUE(caps.wifi_prov);
    TEST_ASSERT_TRUE(caps.wifi_scan);
    TEST_ASSERT_FALSE(caps.thread_prov);
    TEST_ASSERT_TRUE(caps.no_pop);
    TEST_ASSERT_EQUAL(1, caps.sec_ver);

    memset(&caps, 0, sizeof(caps));
    json = "{\"prov\":{\"ver\":\"v1.1\",\"sec_ver\":2,\"cap\":[\"thread_scan\",\"thread_prov\"]}}";
    TEST_ASSERT_EQUAL(ESP_OK, network_provisioner_parse_capabilities(json, &caps));
    TEST_ASSERT_TRUE(caps.thread_prov);
    TEST_ASSERT_TRUE(caps.thread_scan);
    TEST_ASSERT_FALSE(caps.wifi_prov);
    TEST_ASSERT_EQUAL(2, caps.sec_ver);

    memset(&caps, 0, sizeof(caps));
    json = "{\"prov\":{\"ver\":\"v1.1\",\"sec_ver\":0,\"cap\":[\"no_sec\",\"wifi_prov\"]}}";
    TEST_ASSERT_EQUAL(ESP_OK, network_provisioner_parse_capabilities(json, &caps));
    TEST_ASSERT_TRUE(caps.no_sec);
    TEST_ASSERT_EQUAL(0, caps.sec_ver);

    memset(&caps, 0, sizeof(caps));
    /* Missing sec_ver in structured JSON is rejected (no silent Sec0). */
    json = "{\"prov\":{\"ver\":\"v1.1\",\"cap\":[\"no_sec\",\"wifi_prov\"]}}";
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_RESPONSE,
                      network_provisioner_parse_capabilities(json, &caps));
}

TEST_CASE("capabilities: ignore tokens and sec_ver outside prov object", "[network_provisioner]")
{
    network_provisioner_capabilities_t caps = {0};
    const char *json =
        "{\"note\":\"wifi_prov no_sec\",\"prov\":{\"ver\":\"v1.1\",\"sec_ver\":1,\"cap\":[\"wifi_scan\"]}}";
    TEST_ASSERT_EQUAL(ESP_OK, network_provisioner_parse_capabilities(json, &caps));
    /* IDF wifi_provisioning: wifi_scan implies wifi_prov when no Thread caps. */
    TEST_ASSERT_TRUE(caps.wifi_prov);
    TEST_ASSERT_FALSE(caps.no_sec);
    TEST_ASSERT_TRUE(caps.wifi_scan);
    TEST_ASSERT_EQUAL(1, caps.sec_ver);

    memset(&caps, 0, sizeof(caps));
    json = "{\"sec_ver\":0,\"prov\":{\"ver\":\"v1.1\",\"sec_ver\":2,\"cap\":[\"wifi_prov\"]}}";
    TEST_ASSERT_EQUAL(ESP_OK, network_provisioner_parse_capabilities(json, &caps));
    TEST_ASSERT_EQUAL(2, caps.sec_ver);
}

TEST_CASE("capabilities: invalid sec_ver rejected", "[network_provisioner]")
{
    network_provisioner_capabilities_t caps = {0};
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_RESPONSE,
                      network_provisioner_parse_capabilities(
                          "{\"prov\":{\"sec_ver\":9,\"cap\":[\"wifi_prov\"]}}", &caps));
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_RESPONSE,
                      network_provisioner_parse_capabilities(
                          "{\"prov\":{\"sec_ver\":\"x\",\"cap\":[\"wifi_prov\"]}}", &caps));
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_RESPONSE,
                      network_provisioner_parse_capabilities(
                          "{\"prov\":{\"ver\":\"v1.1\"}}", &caps));
}

TEST_CASE("capabilities: bare version string legacy fallback", "[network_provisioner]")
{
    network_provisioner_capabilities_t caps = {0};
    TEST_ASSERT_EQUAL(ESP_OK, network_provisioner_parse_capabilities("v1.0", &caps));
    TEST_ASSERT_TRUE(caps.wifi_prov);
    TEST_ASSERT_TRUE(caps.wifi_scan);
    TEST_ASSERT_EQUAL(1, caps.sec_ver);
}

TEST_CASE("protobuf: SetWifiConfig pack/unpack", "[network_provisioner]")
{
    const char ssid[] = "test-ssid";
    const char pass[] = "test-pass";

    NetworkConfigPayload msg = NETWORK_CONFIG_PAYLOAD__INIT;
    CmdSetWifiConfig cmd = CMD_SET_WIFI_CONFIG__INIT;
    msg.msg = NETWORK_CONFIG_MSG_TYPE__TypeCmdSetWifiConfig;
    msg.payload_case = NETWORK_CONFIG_PAYLOAD__PAYLOAD_CMD_SET_WIFI_CONFIG;
    msg.cmd_set_wifi_config = &cmd;
    cmd.ssid.data = (uint8_t *)ssid;
    cmd.ssid.len = sizeof(ssid) - 1;
    cmd.passphrase.data = (uint8_t *)pass;
    cmd.passphrase.len = sizeof(pass) - 1;

    size_t len = network_config_payload__get_packed_size(&msg);
    uint8_t *buf = malloc(len);
    TEST_ASSERT_NOT_NULL(buf);
    network_config_payload__pack(&msg, buf);

    NetworkConfigPayload *out = network_config_payload__unpack(NULL, len, buf);
    free(buf);
    TEST_ASSERT_NOT_NULL(out);
    TEST_ASSERT_EQUAL(NETWORK_CONFIG_MSG_TYPE__TypeCmdSetWifiConfig, out->msg);
    TEST_ASSERT_NOT_NULL(out->cmd_set_wifi_config);
    TEST_ASSERT_EQUAL(sizeof(ssid) - 1, out->cmd_set_wifi_config->ssid.len);
    TEST_ASSERT_EQUAL_MEMORY(ssid, out->cmd_set_wifi_config->ssid.data, sizeof(ssid) - 1);
    network_config_payload__free_unpacked(out, NULL);
}

TEST_CASE("network_provisioner wraps http protocomm_ext", "[network_provisioner]")
{
    static const esp_http_client_config_t http_cfg = {
        .url = "http://127.0.0.1",
        .timeout_ms = 50,
    };
    protocomm_ext_config_data_t cfg = {
        .transport_method = PROTOCOMM_EXT_TRANSPORT_METHOD_HTTP,
        .security_method = PROTOCOMM_EXT_SECURITY_METHOD_NONE,
        .transport_data = (void *) &http_cfg,
        .security_data = NULL,
    };
    protocomm_ext_t *pc = protocomm_ext_new(&cfg);
    TEST_ASSERT_NOT_NULL(pc);

    network_provisioner_t *np = network_provisioner_create(pc);
    TEST_ASSERT_NOT_NULL(np);
    TEST_ASSERT_EQUAL_PTR(pc, network_provisioner_get_protocomm(np));

    network_provisioner_capabilities_t caps = {0};
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_STATE, network_provisioner_get_capabilities(np, &caps));
    uint8_t *out = NULL;
    size_t out_len = 0;
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_STATE,
                      network_provisioner_send_ep(np, NETWORK_PROVISIONER_EP_CONFIG,
                                                  (const uint8_t *)"x", 1, &out, &out_len));

    network_provisioner_delete(np);
    protocomm_ext_delete(pc);
}

void setUp(void)
{
    unity_utils_record_free_mem();
}

void tearDown(void)
{
    unity_utils_evaluate_leaks_direct(LEAKS_THRESHOLD_BYTES);
}

void app_main(void)
{
    printf("Press ENTER to see the list of tests.\n");
    unity_run_menu();
}
