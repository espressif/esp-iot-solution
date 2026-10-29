/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
/*
 * SPDX-License-Identifier: Apache-2.0
 */

#include "unity.h"
#include "esp_gmp_frame.h"
#include "esp_gmp_ota_proto.h"
#include "esp_gmp_sha256.h"
#include "esp_gmp_types.h"
#include <string.h>

TEST_CASE("gmp frame round-trip", "[esp_gmp]")
{
    uint8_t payload[] = { 0xde, 0xad };
    uint8_t buf[32];

    size_t n = esp_gmp_frame_build(buf, sizeof(buf), ESP_GMP_VER, ESP_GMP_OP_WRITE_REQ,
                                   ESP_GMP_GRP_OTA, 0x1234, ESP_GMP_OTA_UPLOAD_DATA, 0, 0, payload,
                                   sizeof(payload));
    TEST_ASSERT_EQUAL(12, n);

    esp_gmp_parsed_t p;
    TEST_ASSERT_EQUAL(0, esp_gmp_frame_parse(buf, n, &p));
    TEST_ASSERT_EQUAL(ESP_GMP_VER, p.ver);
    TEST_ASSERT_EQUAL(ESP_GMP_OP_WRITE_REQ, p.op);
    TEST_ASSERT_EQUAL(ESP_GMP_GRP_OTA, p.group_id);
    TEST_ASSERT_EQUAL(0x1234, p.seq_host);
    TEST_ASSERT_EQUAL(0, p.reserved0);
    TEST_ASSERT_EQUAL(ESP_GMP_OTA_UPLOAD_DATA, p.command_id);
    TEST_ASSERT_EQUAL(2, p.packet_length_host);
    TEST_ASSERT_EQUAL(2, p.payload_len);
    TEST_ASSERT_EQUAL_HEX8_ARRAY(payload, p.payload, 2);
}

TEST_CASE("gmp ota data parser validates lengths", "[esp_gmp][ota]")
{
    uint8_t payload[ESP_GMP_OTA_DATA_HDR_LEN + 4] = {
        1, 0, 0, 4, 0, 0, 0, 0, 0xde, 0xad, 0xbe, 0xef
    };
    esp_gmp_ota_data_req_t req;

    TEST_ASSERT_EQUAL(sizeof(payload), esp_gmp_ota_data_parse_req(payload, sizeof(payload), &req));
    TEST_ASSERT_EQUAL(1, req.session_id);
    TEST_ASSERT_EQUAL(4, req.data_len);
    TEST_ASSERT_EQUAL(0, req.image_offset);
    TEST_ASSERT_EQUAL_HEX8_ARRAY(&payload[ESP_GMP_OTA_DATA_HDR_LEN], req.data, 4);

    payload[2] = 0;
    payload[3] = 5;
    TEST_ASSERT_EQUAL(0, esp_gmp_ota_data_parse_req(payload, sizeof(payload), &req));
}

TEST_CASE("gmp ota finish requires sha256 flag", "[esp_gmp][ota]")
{
    uint8_t payload[ESP_GMP_OTA_CTRL_FINISH_LEN] = { 0 };
    esp_gmp_ota_ctrl_req_t req;

    payload[0] = ESP_GMP_OTA_CTRL_ACTION_FINISH;
    payload[1] = 7;
    payload[2] = (uint8_t)(ESP_GMP_OTA_CTRL_FLAG_SHA256 >> 8);
    payload[3] = (uint8_t)ESP_GMP_OTA_CTRL_FLAG_SHA256;
    TEST_ASSERT_TRUE(esp_gmp_ota_ctrl_parse_req(payload, sizeof(payload), &req));
    TEST_ASSERT_TRUE(req.has_sha256);
    TEST_ASSERT_EQUAL(7, req.session_id);

    payload[3] = 0;
    TEST_ASSERT_FALSE(esp_gmp_ota_ctrl_parse_req(payload, sizeof(payload), &req));
}

TEST_CASE("gmp sha256 incremental matches digest", "[esp_gmp][sha256]")
{
    const uint8_t data[] = "esp-gmp-sha256";
    uint8_t one_shot[32];
    uint8_t incremental[32];
    esp_gmp_sha256_ctx_t ctx;

    TEST_ASSERT_EQUAL(ESP_OK, esp_gmp_sha256_digest(data, sizeof(data) - 1, one_shot));
    TEST_ASSERT_EQUAL(ESP_OK, esp_gmp_sha256_begin(&ctx));
    TEST_ASSERT_EQUAL(ESP_OK, esp_gmp_sha256_update(&ctx, data, 7));
    TEST_ASSERT_EQUAL(ESP_OK, esp_gmp_sha256_update(&ctx, data + 7, sizeof(data) - 1 - 7));
    TEST_ASSERT_EQUAL(ESP_OK, esp_gmp_sha256_finish(&ctx, incremental));
    TEST_ASSERT_EQUAL_HEX8_ARRAY(one_shot, incremental, sizeof(one_shot));
}

void app_main(void)
{
    unity_run_menu();
}
