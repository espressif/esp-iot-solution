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

#include "esp_local_ctrl.pb-c.h"
#include "esp_local_controller.h"
#include "esp_local_controller_priv.h"
#include "protocomm_ext.h"
#include "protocomm_ext_security.h"

#define LEAKS_THRESHOLD_BYTES (400)

TEST_CASE("esp_local_controller_create: NULL pc returns NULL", "[esp_local_controller]")
{
    TEST_ASSERT_NULL(esp_local_controller_create(NULL));
}

TEST_CASE("version: parse local_ctrl JSON", "[esp_local_controller]")
{
    esp_local_controller_version_t ver = {0};
    const char *json =
        "{\"local_ctrl\":{\"ver\":\"v1.0\",\"sec_ver\":1,\"sec_patch_ver\":0}}";
    TEST_ASSERT_EQUAL(ESP_OK, esp_local_controller_parse_version(json, &ver));
    TEST_ASSERT_EQUAL_STRING("v1.0", ver.ver);
    TEST_ASSERT_EQUAL(1, ver.sec_ver);
    TEST_ASSERT_EQUAL(0, ver.sec_patch_ver);
    esp_local_controller_version_free(&ver);

    memset(&ver, 0, sizeof(ver));
    json = "{\"local_ctrl\":{\"ver\":\"v1.1\",\"sec_ver\":2,\"sec_patch_ver\":1}}";
    TEST_ASSERT_EQUAL(ESP_OK, esp_local_controller_parse_version(json, &ver));
    TEST_ASSERT_EQUAL(2, ver.sec_ver);
    TEST_ASSERT_EQUAL(1, ver.sec_patch_ver);
    esp_local_controller_version_free(&ver);
}

TEST_CASE("version: invalid sec_ver rejected", "[esp_local_controller]")
{
    esp_local_controller_version_t ver = {0};
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_RESPONSE,
                      esp_local_controller_parse_version(
                          "{\"local_ctrl\":{\"ver\":\"v1\",\"sec_ver\":9}}", &ver));
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_RESPONSE,
                      esp_local_controller_parse_version(
                          "{\"local_ctrl\":{\"ver\":\"v1\"}}", &ver));
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_RESPONSE,
                      esp_local_controller_parse_version("v1.0", &ver));
}

TEST_CASE("version: ignores sec_ver outside local_ctrl object", "[esp_local_controller]")
{
    esp_local_controller_version_t ver = {0};
    /* Injected outer sec_ver must not win over the value inside local_ctrl. */
    const char *json =
        "{\"sec_ver\":0,\"local_ctrl\":{\"ver\":\"v1.0\",\"sec_ver\":1,\"sec_patch_ver\":0}}";
    TEST_ASSERT_EQUAL(ESP_OK, esp_local_controller_parse_version(json, &ver));
    TEST_ASSERT_EQUAL(1, ver.sec_ver);
    esp_local_controller_version_free(&ver);

    memset(&ver, 0, sizeof(ver));
    json = "{\"local_ctrl\":{\"ver\":\"v1.0\",\"sec_ver\":2,\"sec_patch_ver\":1},\"sec_ver\":0}";
    TEST_ASSERT_EQUAL(ESP_OK, esp_local_controller_parse_version(json, &ver));
    TEST_ASSERT_EQUAL(2, ver.sec_ver);
    esp_local_controller_version_free(&ver);
}

TEST_CASE("protobuf: GetPropertyCount pack/unpack", "[esp_local_controller]")
{
    LocalCtrlMessage msg = LOCAL_CTRL_MESSAGE__INIT;
    CmdGetPropertyCount cmd = CMD_GET_PROPERTY_COUNT__INIT;
    msg.msg = LOCAL_CTRL_MSG_TYPE__TypeCmdGetPropertyCount;
    msg.payload_case = LOCAL_CTRL_MESSAGE__PAYLOAD_CMD_GET_PROP_COUNT;
    msg.cmd_get_prop_count = &cmd;

    size_t len = local_ctrl_message__get_packed_size(&msg);
    uint8_t *buf = malloc(len);
    TEST_ASSERT_NOT_NULL(buf);
    local_ctrl_message__pack(&msg, buf);

    LocalCtrlMessage *out = local_ctrl_message__unpack(NULL, len, buf);
    free(buf);
    TEST_ASSERT_NOT_NULL(out);
    TEST_ASSERT_EQUAL(LOCAL_CTRL_MSG_TYPE__TypeCmdGetPropertyCount, out->msg);
    TEST_ASSERT_NOT_NULL(out->cmd_get_prop_count);
    local_ctrl_message__free_unpacked(out, NULL);
}

TEST_CASE("esp_local_controller wraps http protocomm_ext", "[esp_local_controller]")
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

    esp_local_controller_t *ctrl = esp_local_controller_create(pc);
    TEST_ASSERT_NOT_NULL(ctrl);
    TEST_ASSERT_EQUAL_PTR(pc, esp_local_controller_get_protocomm(ctrl));

    esp_local_controller_version_t ver = {0};
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_STATE, esp_local_controller_get_version(ctrl, &ver));
    uint8_t *out = NULL;
    size_t out_len = 0;
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_STATE,
                      esp_local_controller_send_ep(ctrl, ESP_LOCAL_CONTROLLER_EP_CONTROL,
                                                   (const uint8_t *)"x", 1, &out, &out_len));

    esp_local_controller_delete(ctrl);
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
