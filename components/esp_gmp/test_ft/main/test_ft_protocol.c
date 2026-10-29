/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <string.h>
#include "unity.h"
#include "esp_gmp_ft_proto.h"
#include "esp_file_transfer_data_pipe.h"

#define TEST_TRANSFER_ID 0x12345678
#define TEST_BLOCK_INDEX 42
#define TEST_FILE_SIZE   0x0000000000ABCDEFULL
#define TEST_BLOCK_SIZE  4096

/**
 * Golden frame test data - wire format baseline.
 *
 * These frames capture the exact byte layout expected on the wire.
 * Any change that breaks these tests indicates a protocol compatibility break.
 */

/* Golden META_REQUEST frame (minimal file name) */
static const uint8_t golden_meta_req[] = {
    /* transfer_id (BE) */
    0x12, 0x34, 0x56, 0x78,
    /* file_size (BE 64-bit) */
    0x00, 0x00, 0x00, 0x00, 0x00, 0xAB, 0xCD, 0xEF,
    /* block_size (BE) */
    0x00, 0x00, 0x10, 0x00,
    /* total_blocks (BE): ceil(0xABCDEF / 4096) = 0x0ABD */
    0x00, 0x00, 0x0A, 0xBD,
    /* sha256[32] */
    0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08,
    0x09, 0x0A, 0x0B, 0x0C, 0x0D, 0x0E, 0x0F, 0x10,
    0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17, 0x18,
    0x19, 0x1A, 0x1B, 0x1C, 0x1D, 0x1E, 0x1F, 0x20,
    /* reserved (BE) */
    0x00, 0x00,
    /* name_len */
    0x08,
    /* pad byte */
    0x00,
    /* file_name */
    't', 'e', 's', 't', '.', 't', 'x', 't',
};

/* Golden META_RESPONSE frame */
static const uint8_t golden_meta_rsp[] = {
    /* transfer_id (BE) */
    0x12, 0x34, 0x56, 0x78,
    /* status */
    0x00,
    /* pad byte */
    0x00,
    /* reason_code (BE) */
    0x00, 0x00,
    /* accepted_block_size (BE) */
    0x00, 0x00, 0x10, 0x00,
};

/* Golden DATA_REQUEST header (12 bytes) */
static const uint8_t golden_data_req_hdr[] = {
    /* transfer_id (BE) */
    0x12, 0x34, 0x56, 0x78,
    /* block_index (BE) */
    0x00, 0x00, 0x00, 0x2A,
    /* data_len (BE) */
    0x00, 0x00, 0x04, 0x00,
};

/* Golden DATA_RESPONSE frame */
static const uint8_t golden_data_rsp[] = {
    /* transfer_id (BE) */
    0x12, 0x34, 0x56, 0x78,
    /* block_index (BE) */
    0x00, 0x00, 0x00, 0x2A,
    /* status */
    0x00,
    /* pad byte */
    0x00,
    /* reason_code (BE) */
    0x00, 0x00,
};

/* Golden FINAL_CONFIRM frame */
static const uint8_t golden_final_confirm[] = {
    /* transfer_id (BE) */
    0x12, 0x34, 0x56, 0x78,
    /* status */
    0x00,
    /* name_len */
    0x08,
    /* reason_code (BE) */
    0x00, 0x00,
    /* received_size (BE 64-bit) */
    0x00, 0x00, 0x00, 0x00, 0x00, 0xAB, 0xCD, 0xEF,
    /* received_sha256[32] */
    0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08,
    0x09, 0x0A, 0x0B, 0x0C, 0x0D, 0x0E, 0x0F, 0x10,
    0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17, 0x18,
    0x19, 0x1A, 0x1B, 0x1C, 0x1D, 0x1E, 0x1F, 0x20,
    /* saved_name */
    't', 'e', 's', 't', '.', 't', 'x', 't',
};

/* Golden ABORT frame */
static const uint8_t golden_abort[] = {
    /* transfer_id (BE) */
    0x12, 0x34, 0x56, 0x78,
    /* reason_code (BE) */
    0x00, 0x0E,
    /* reserved (BE) */
    0x00, 0x00,
};

TEST_CASE("ft_protocol: META_REQUEST encode matches golden frame", "[esp_file_transfer]")
{
    ft_meta_request_t req = {
        .transfer_id = TEST_TRANSFER_ID,
        .file_size = TEST_FILE_SIZE,
        .block_size = TEST_BLOCK_SIZE,
        .total_blocks = 0x0ABD, /* must match ceil(file_size / block_size) */
    };

    for (int i = 0; i < 32; i++) {
        req.sha256[i] = i + 1;
    }
    strcpy(req.file_name, "test.txt");

    uint8_t buf[ESP_FT_META_REQ_MAX_LEN];
    size_t encoded_len = 0;

    esp_err_t err = ft_protocol_encode_meta_request(&req, buf, sizeof(buf), &encoded_len);
    TEST_ASSERT_EQUAL(ESP_OK, err);
    TEST_ASSERT_EQUAL(sizeof(golden_meta_req), encoded_len);
    TEST_ASSERT_EQUAL_HEX8_ARRAY(golden_meta_req, buf, encoded_len);
}

TEST_CASE("ft_protocol: META_REQUEST decode from golden frame", "[esp_file_transfer]")
{
    ft_meta_request_t req = {0};

    esp_err_t err = ft_protocol_decode_meta_request(golden_meta_req, sizeof(golden_meta_req), &req);
    TEST_ASSERT_EQUAL(ESP_OK, err);
    TEST_ASSERT_EQUAL_UINT32(TEST_TRANSFER_ID, req.transfer_id);
    TEST_ASSERT_EQUAL_UINT64(TEST_FILE_SIZE, req.file_size);
    TEST_ASSERT_EQUAL_UINT32(TEST_BLOCK_SIZE, req.block_size);
    TEST_ASSERT_EQUAL_UINT32(0x0ABD, req.total_blocks);

    for (int i = 0; i < 32; i++) {
        TEST_ASSERT_EQUAL_UINT8(i + 1, req.sha256[i]);
    }
    TEST_ASSERT_EQUAL_STRING("test.txt", req.file_name);
}

TEST_CASE("ft_protocol: META_RESPONSE encode matches golden frame", "[esp_file_transfer]")
{
    ft_meta_response_t rsp = {
        .transfer_id = TEST_TRANSFER_ID,
        .status = ESP_FT_WIRE_STATUS_OK,
        .reason_code = ESP_FT_REASON_OK,
        .accepted_block_size = TEST_BLOCK_SIZE,
    };

    uint8_t buf[ESP_FT_META_RSP_LEN];

    esp_err_t err = ft_protocol_encode_meta_response(&rsp, buf, sizeof(buf));
    TEST_ASSERT_EQUAL(ESP_OK, err);
    TEST_ASSERT_EQUAL_HEX8_ARRAY(golden_meta_rsp, buf, sizeof(golden_meta_rsp));
}

TEST_CASE("ft_protocol: META_RESPONSE decode from golden frame", "[esp_file_transfer]")
{
    ft_meta_response_t rsp = {0};

    esp_err_t err = ft_protocol_decode_meta_response(golden_meta_rsp, sizeof(golden_meta_rsp), &rsp);
    TEST_ASSERT_EQUAL(ESP_OK, err);
    TEST_ASSERT_EQUAL_UINT32(TEST_TRANSFER_ID, rsp.transfer_id);
    TEST_ASSERT_EQUAL_UINT8(ESP_FT_WIRE_STATUS_OK, rsp.status);
    TEST_ASSERT_EQUAL_UINT16(ESP_FT_REASON_OK, rsp.reason_code);
    TEST_ASSERT_EQUAL_UINT32(TEST_BLOCK_SIZE, rsp.accepted_block_size);
}

TEST_CASE("ft_protocol: DATA_REQUEST header encode matches golden frame", "[esp_file_transfer]")
{
    uint8_t buf[ESP_FT_DATA_REQ_HDR_LEN];

    ft_protocol_encode_data_request_header(buf, TEST_TRANSFER_ID, TEST_BLOCK_INDEX, 1024);

    TEST_ASSERT_EQUAL_HEX8_ARRAY(golden_data_req_hdr, buf, sizeof(golden_data_req_hdr));
}

TEST_CASE("ft_protocol: DATA_REQUEST decode from golden frame", "[esp_file_transfer]")
{
    /* Decode requires header + payload whose length matches data_len. */
    uint8_t frame[ESP_FT_DATA_REQ_HDR_LEN + 1024];
    memset(frame, 0xA5, sizeof(frame));
    memcpy(frame, golden_data_req_hdr, sizeof(golden_data_req_hdr));

    ft_data_request_t req = {0};
    esp_err_t err = ft_protocol_decode_data_request(frame, sizeof(frame), &req);
    TEST_ASSERT_EQUAL(ESP_OK, err);
    TEST_ASSERT_EQUAL_UINT32(TEST_TRANSFER_ID, req.transfer_id);
    TEST_ASSERT_EQUAL_UINT32(TEST_BLOCK_INDEX, req.block_index);
    TEST_ASSERT_EQUAL_UINT32(1024, req.data_len);
    TEST_ASSERT_EQUAL_PTR(frame + ESP_FT_DATA_REQ_HDR_LEN, req.data);
}

TEST_CASE("ft_protocol: DATA_RESPONSE encode matches golden frame", "[esp_file_transfer]")
{
    ft_data_response_t rsp = {
        .transfer_id = TEST_TRANSFER_ID,
        .block_index = TEST_BLOCK_INDEX,
        .status = ESP_FT_WIRE_STATUS_OK,
        .reason_code = ESP_FT_REASON_OK,
    };

    uint8_t buf[ESP_FT_DATA_RSP_LEN];

    esp_err_t err = ft_protocol_encode_data_response(&rsp, buf, sizeof(buf));
    TEST_ASSERT_EQUAL(ESP_OK, err);
    TEST_ASSERT_EQUAL_HEX8_ARRAY(golden_data_rsp, buf, sizeof(golden_data_rsp));
}

TEST_CASE("ft_protocol: DATA_RESPONSE decode from golden frame", "[esp_file_transfer]")
{
    ft_data_response_t rsp = {0};

    esp_err_t err = ft_protocol_decode_data_response(golden_data_rsp,
                                                     sizeof(golden_data_rsp), &rsp);
    TEST_ASSERT_EQUAL(ESP_OK, err);
    TEST_ASSERT_EQUAL_UINT32(TEST_TRANSFER_ID, rsp.transfer_id);
    TEST_ASSERT_EQUAL_UINT32(TEST_BLOCK_INDEX, rsp.block_index);
    TEST_ASSERT_EQUAL_UINT8(ESP_FT_WIRE_STATUS_OK, rsp.status);
    TEST_ASSERT_EQUAL_UINT16(ESP_FT_REASON_OK, rsp.reason_code);
}

TEST_CASE("ft_protocol: FINAL_CONFIRM encode matches golden frame", "[esp_file_transfer]")
{
    ft_final_confirm_t confirm = {
        .transfer_id = TEST_TRANSFER_ID,
        .status = ESP_FT_WIRE_STATUS_OK,
        .reason_code = ESP_FT_REASON_OK,
        .received_size = TEST_FILE_SIZE,
    };

    for (int i = 0; i < 32; i++) {
        confirm.received_sha256[i] = i + 1;
    }
    strcpy(confirm.saved_name, "test.txt");

    uint8_t buf[ESP_FT_FINAL_MAX_LEN];
    size_t encoded_len = 0;

    esp_err_t err = ft_protocol_encode_final_confirm(&confirm, buf, sizeof(buf), &encoded_len);
    TEST_ASSERT_EQUAL(ESP_OK, err);
    TEST_ASSERT_EQUAL(sizeof(golden_final_confirm), encoded_len);
    TEST_ASSERT_EQUAL_HEX8_ARRAY(golden_final_confirm, buf, encoded_len);
}

TEST_CASE("ft_protocol: FINAL_CONFIRM decode from golden frame", "[esp_file_transfer]")
{
    ft_final_confirm_t confirm = {0};

    esp_err_t err = ft_protocol_decode_final_confirm(golden_final_confirm,
                                                     sizeof(golden_final_confirm), &confirm);
    TEST_ASSERT_EQUAL(ESP_OK, err);
    TEST_ASSERT_EQUAL_UINT32(TEST_TRANSFER_ID, confirm.transfer_id);
    TEST_ASSERT_EQUAL_UINT8(ESP_FT_WIRE_STATUS_OK, confirm.status);
    TEST_ASSERT_EQUAL_UINT16(ESP_FT_REASON_OK, confirm.reason_code);
    TEST_ASSERT_EQUAL_UINT64(TEST_FILE_SIZE, confirm.received_size);

    for (int i = 0; i < 32; i++) {
        TEST_ASSERT_EQUAL_UINT8(i + 1, confirm.received_sha256[i]);
    }
    TEST_ASSERT_EQUAL_STRING("test.txt", confirm.saved_name);
}

TEST_CASE("ft_protocol: ABORT encode matches golden frame", "[esp_file_transfer]")
{
    ft_abort_t abort = {
        .transfer_id = TEST_TRANSFER_ID,
        .reason_code = ESP_FT_REASON_ABORTED,
    };

    uint8_t buf[ESP_FT_ABORT_LEN];

    esp_err_t err = ft_protocol_encode_abort(&abort, buf, sizeof(buf));
    TEST_ASSERT_EQUAL(ESP_OK, err);
    TEST_ASSERT_EQUAL_HEX8_ARRAY(golden_abort, buf, sizeof(golden_abort));
}

TEST_CASE("ft_protocol: ABORT decode from golden frame", "[esp_file_transfer]")
{
    ft_abort_t abort = {0};

    esp_err_t err = ft_protocol_decode_abort(golden_abort, sizeof(golden_abort), &abort);
    TEST_ASSERT_EQUAL(ESP_OK, err);
    TEST_ASSERT_EQUAL_UINT32(TEST_TRANSFER_ID, abort.transfer_id);
    TEST_ASSERT_EQUAL_UINT16(ESP_FT_REASON_ABORTED, abort.reason_code);
}

TEST_CASE("ft_protocol: round-trip encode/decode preserves data", "[esp_file_transfer]")
{
    /* Test META_REQUEST round-trip */
    ft_meta_request_t orig_meta = {
        .transfer_id = 0xDEADBEEF,
        .file_size = 8192ULL * 1024ULL, /* consistent with total_blocks */
        .block_size = 8192,
        .total_blocks = 1024,
    };
    memset(orig_meta.sha256, 0xAB, 32);
    strcpy(orig_meta.file_name, "roundtrip.bin");

    uint8_t meta_buf[ESP_FT_META_REQ_MAX_LEN];
    size_t meta_len = 0;
    TEST_ASSERT_EQUAL(ESP_OK, ft_protocol_encode_meta_request(&orig_meta, meta_buf,
                                                              sizeof(meta_buf), &meta_len));

    ft_meta_request_t decoded_meta = {0};
    TEST_ASSERT_EQUAL(ESP_OK, ft_protocol_decode_meta_request(meta_buf, meta_len, &decoded_meta));

    TEST_ASSERT_EQUAL_UINT32(orig_meta.transfer_id, decoded_meta.transfer_id);
    TEST_ASSERT_EQUAL_UINT64(orig_meta.file_size, decoded_meta.file_size);
    TEST_ASSERT_EQUAL_UINT32(orig_meta.block_size, decoded_meta.block_size);
    TEST_ASSERT_EQUAL_UINT32(orig_meta.total_blocks, decoded_meta.total_blocks);
    TEST_ASSERT_EQUAL_MEMORY(orig_meta.sha256, decoded_meta.sha256, 32);
    TEST_ASSERT_EQUAL_STRING(orig_meta.file_name, decoded_meta.file_name);
}

TEST_CASE("ft_protocol: decode rejects truncated META_REQUEST and ABORT", "[esp_file_transfer]")
{
    ft_meta_request_t meta = {0};
    ft_abort_t abort = {0};

    /* META_REQUEST shorter than fixed header */
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG,
                      ft_protocol_decode_meta_request(golden_meta_req,
                                                      ESP_FT_META_REQ_HDR_LEN - 1, &meta));
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG,
                      ft_protocol_decode_meta_request(golden_meta_req, 0, &meta));

    /* META_REQUEST header present but name payload truncated vs name_len */
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_SIZE,
                      ft_protocol_decode_meta_request(golden_meta_req,
                                                      ESP_FT_META_REQ_HDR_LEN, &meta));

    /* ABORT shorter / longer than fixed frame */
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_SIZE,
                      ft_protocol_decode_abort(golden_abort, ESP_FT_ABORT_LEN - 1, &abort));
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_SIZE,
                      ft_protocol_decode_abort(golden_abort, 0, &abort));
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_SIZE,
                      ft_protocol_decode_abort(golden_abort, ESP_FT_ABORT_LEN + 1, &abort));
}

TEST_CASE("ft_protocol: ABORT reason codes for link-down and abort paths", "[esp_file_transfer]")
{
    /*
     * Production abort/link-down paths use these reason codes on the wire:
     * - ESP_FT_REASON_LINK_ERROR (link_down / secondary / session)
     * - ESP_FT_REASON_ABORTED (local cancel / peer abort)
     */
    static const uint16_t reasons[] = {
        ESP_FT_REASON_LINK_ERROR,
        ESP_FT_REASON_ABORTED,
    };

    for (size_t i = 0; i < sizeof(reasons) / sizeof(reasons[0]); i++) {
        ft_abort_t abort = {
            .transfer_id = TEST_TRANSFER_ID,
            .reason_code = reasons[i],
        };
        uint8_t buf[ESP_FT_ABORT_LEN];
        memset(buf, 0xFF, sizeof(buf));

        TEST_ASSERT_EQUAL(ESP_OK, ft_protocol_encode_abort(&abort, buf, sizeof(buf)));
        TEST_ASSERT_EQUAL_HEX8(0x12, buf[0]);
        TEST_ASSERT_EQUAL_HEX8(0x34, buf[1]);
        TEST_ASSERT_EQUAL_HEX8(0x56, buf[2]);
        TEST_ASSERT_EQUAL_HEX8(0x78, buf[3]);
        TEST_ASSERT_EQUAL_HEX8((uint8_t)(reasons[i] >> 8), buf[4]);
        TEST_ASSERT_EQUAL_HEX8((uint8_t)(reasons[i] & 0xFF), buf[5]);
        TEST_ASSERT_EQUAL_HEX8(0x00, buf[6]);
        TEST_ASSERT_EQUAL_HEX8(0x00, buf[7]);

        ft_abort_t decoded = {0};
        TEST_ASSERT_EQUAL(ESP_OK, ft_protocol_decode_abort(buf, sizeof(buf), &decoded));
        TEST_ASSERT_EQUAL_UINT32(TEST_TRANSFER_ID, decoded.transfer_id);
        TEST_ASSERT_EQUAL_UINT16(reasons[i], decoded.reason_code);
    }

    /* ESP_FT_REASON_OK is not a valid ABORT reason */
    ft_abort_t ok_abort = {
        .transfer_id = TEST_TRANSFER_ID,
        .reason_code = ESP_FT_REASON_OK,
    };
    uint8_t discard[ESP_FT_ABORT_LEN];
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG,
                      ft_protocol_encode_abort(&ok_abort, discard, sizeof(discard)));
}

/**
 * Happy-path wire sequence (encode-only, state-machine-free).
 *
 * Host-side baseline for META → DATA → FINAL → ABORT framing when full
 * FreeRTOS FT init + GMP mock is not practical in this unit test app.
 * Asserts each encode step yields expected sizes and key fields.
 */
TEST_CASE("ft_protocol: happy path wire sequence META/DATA/FINAL/ABORT", "[esp_file_transfer]")
{
    const uint32_t tid = TEST_TRANSFER_ID;
    const uint32_t block_size = TEST_BLOCK_SIZE;
    const uint32_t data_len = 1024;
    const uint32_t block_index = 0;
    const uint64_t file_size = data_len;
    const uint32_t total_blocks = 1;

    /* 1) META_REQUEST */
    ft_meta_request_t meta_req = {
        .transfer_id = tid,
        .file_size = file_size,
        .block_size = block_size,
        .total_blocks = total_blocks,
    };
    memset(meta_req.sha256, 0x11, sizeof(meta_req.sha256));
    strcpy(meta_req.file_name, "flow.bin");

    uint8_t meta_req_buf[ESP_FT_META_REQ_MAX_LEN];
    size_t meta_req_len = 0;
    TEST_ASSERT_EQUAL(ESP_OK, ft_protocol_encode_meta_request(&meta_req, meta_req_buf,
                                                              sizeof(meta_req_buf),
                                                              &meta_req_len));
    TEST_ASSERT_EQUAL(ESP_FT_META_REQ_HDR_LEN + strlen("flow.bin"), meta_req_len);
    TEST_ASSERT_EQUAL_HEX8(0x12, meta_req_buf[0]);
    TEST_ASSERT_EQUAL_HEX8(0x34, meta_req_buf[1]);
    TEST_ASSERT_EQUAL_HEX8(0x56, meta_req_buf[2]);
    TEST_ASSERT_EQUAL_HEX8(0x78, meta_req_buf[3]);

    /* 2) META_RESPONSE */
    ft_meta_response_t meta_rsp = {
        .transfer_id = tid,
        .status = ESP_FT_WIRE_STATUS_OK,
        .reason_code = ESP_FT_REASON_OK,
        .accepted_block_size = block_size,
    };
    uint8_t meta_rsp_buf[ESP_FT_META_RSP_LEN];
    TEST_ASSERT_EQUAL(ESP_OK, ft_protocol_encode_meta_response(&meta_rsp, meta_rsp_buf,
                                                               sizeof(meta_rsp_buf)));
    TEST_ASSERT_EQUAL_UINT8(ESP_FT_WIRE_STATUS_OK, meta_rsp_buf[4]);
    TEST_ASSERT_EQUAL_HEX8(0x00, meta_rsp_buf[6]);
    TEST_ASSERT_EQUAL_HEX8(0x00, meta_rsp_buf[7]); /* reason OK */

    /* 3) DATA_REQUEST header */
    uint8_t data_req_hdr[ESP_FT_DATA_REQ_HDR_LEN];
    ft_protocol_encode_data_request_header(data_req_hdr, tid, block_index, data_len);
    TEST_ASSERT_EQUAL_HEX8(0x00, data_req_hdr[4]);
    TEST_ASSERT_EQUAL_HEX8(0x00, data_req_hdr[5]);
    TEST_ASSERT_EQUAL_HEX8(0x00, data_req_hdr[6]);
    TEST_ASSERT_EQUAL_HEX8(0x00, data_req_hdr[7]); /* block_index 0 */
    TEST_ASSERT_EQUAL_HEX8(0x00, data_req_hdr[8]);
    TEST_ASSERT_EQUAL_HEX8(0x00, data_req_hdr[9]);
    TEST_ASSERT_EQUAL_HEX8(0x04, data_req_hdr[10]);
    TEST_ASSERT_EQUAL_HEX8(0x00, data_req_hdr[11]); /* data_len 1024 */

    /* 4) DATA_RESPONSE */
    ft_data_response_t data_rsp = {
        .transfer_id = tid,
        .block_index = block_index,
        .status = ESP_FT_WIRE_STATUS_OK,
        .reason_code = ESP_FT_REASON_OK,
    };
    uint8_t data_rsp_buf[ESP_FT_DATA_RSP_LEN];
    TEST_ASSERT_EQUAL(ESP_OK, ft_protocol_encode_data_response(&data_rsp, data_rsp_buf,
                                                               sizeof(data_rsp_buf)));
    TEST_ASSERT_EQUAL_UINT8(ESP_FT_WIRE_STATUS_OK, data_rsp_buf[8]);

    /* 5) FINAL_CONFIRM */
    ft_final_confirm_t final_confirm = {
        .transfer_id = tid,
        .status = ESP_FT_WIRE_STATUS_OK,
        .reason_code = ESP_FT_REASON_OK,
        .received_size = file_size,
    };
    memset(final_confirm.received_sha256, 0x11, sizeof(final_confirm.received_sha256));
    strcpy(final_confirm.saved_name, "flow.bin");

    uint8_t final_buf[ESP_FT_FINAL_MAX_LEN];
    size_t final_len = 0;
    TEST_ASSERT_EQUAL(ESP_OK, ft_protocol_encode_final_confirm(&final_confirm, final_buf,
                                                               sizeof(final_buf), &final_len));
    TEST_ASSERT_EQUAL(ESP_FT_FINAL_HDR_LEN + strlen("flow.bin"), final_len);
    TEST_ASSERT_EQUAL_UINT8(ESP_FT_WIRE_STATUS_OK, final_buf[4]);

    /* 6) ABORT (peer/local cancel framing after a completed or interrupted flow) */
    ft_abort_t abort = {
        .transfer_id = tid,
        .reason_code = ESP_FT_REASON_ABORTED,
    };
    uint8_t abort_buf[ESP_FT_ABORT_LEN];
    TEST_ASSERT_EQUAL(ESP_OK, ft_protocol_encode_abort(&abort, abort_buf, sizeof(abort_buf)));
    TEST_ASSERT_EQUAL_HEX8(0x00, abort_buf[4]);
    TEST_ASSERT_EQUAL_HEX8(0x0E, abort_buf[5]); /* ESP_FT_REASON_ABORTED */
}

TEST_CASE("ft_data_pipe: add find clear and capacity", "[esp_file_transfer]")
{
    ft_data_pipe_t pipe;
    ft_data_pipe_reset(&pipe);
    TEST_ASSERT_EQUAL(0, ft_data_pipe_count(&pipe));

    /* Fill pipeline with distinct seq/block pairs (simulates pipelined DATA TX). */
    for (int i = 0; i < 3; i++) {
        int idx = ft_data_pipe_add(&pipe, (uint16_t)(100 + i), (uint32_t)i, 512);
        TEST_ASSERT_TRUE(idx >= 0);
    }
    TEST_ASSERT_EQUAL(3, ft_data_pipe_count(&pipe));
    TEST_ASSERT_TRUE(ft_data_pipe_has_block(&pipe, 1));
    TEST_ASSERT_FALSE(ft_data_pipe_has_block(&pipe, 9));

    int hit = ft_data_pipe_find_seq(&pipe, 101);
    TEST_ASSERT_EQUAL(1, hit);
    TEST_ASSERT_EQUAL_UINT32(1, pipe.slots[hit].block_index);
    TEST_ASSERT_EQUAL_UINT32(512, pipe.slots[hit].data_len);

    ft_data_pipe_clear(&pipe, hit);
    TEST_ASSERT_EQUAL(2, ft_data_pipe_count(&pipe));
    TEST_ASSERT_EQUAL(-1, ft_data_pipe_find_seq(&pipe, 101));
    TEST_ASSERT_FALSE(ft_data_pipe_has_block(&pipe, 1));

    /* Fill to capacity then reject. */
    ft_data_pipe_reset(&pipe);
    for (int i = 0; i < FT_DATA_PIPE_MAX; i++) {
        TEST_ASSERT_TRUE(ft_data_pipe_add(&pipe, (uint16_t)i, (uint32_t)i, 64) >= 0);
    }
    TEST_ASSERT_EQUAL(FT_DATA_PIPE_MAX, ft_data_pipe_count(&pipe));
    TEST_ASSERT_EQUAL(-1, ft_data_pipe_add(&pipe, 0xFFFF, 99, 64));
}

void app_main(void)
{
    UNITY_BEGIN();

    unity_run_test_by_name("ft_protocol: META_REQUEST encode matches golden frame");
    unity_run_test_by_name("ft_protocol: META_REQUEST decode from golden frame");
    unity_run_test_by_name("ft_protocol: META_RESPONSE encode matches golden frame");
    unity_run_test_by_name("ft_protocol: META_RESPONSE decode from golden frame");
    unity_run_test_by_name("ft_protocol: DATA_REQUEST header encode matches golden frame");
    unity_run_test_by_name("ft_protocol: DATA_REQUEST decode from golden frame");
    unity_run_test_by_name("ft_protocol: DATA_RESPONSE encode matches golden frame");
    unity_run_test_by_name("ft_protocol: DATA_RESPONSE decode from golden frame");
    unity_run_test_by_name("ft_protocol: FINAL_CONFIRM encode matches golden frame");
    unity_run_test_by_name("ft_protocol: FINAL_CONFIRM decode from golden frame");
    unity_run_test_by_name("ft_protocol: ABORT encode matches golden frame");
    unity_run_test_by_name("ft_protocol: ABORT decode from golden frame");
    unity_run_test_by_name("ft_protocol: round-trip encode/decode preserves data");
    unity_run_test_by_name("ft_protocol: decode rejects truncated META_REQUEST and ABORT");
    unity_run_test_by_name("ft_protocol: ABORT reason codes for link-down and abort paths");
    unity_run_test_by_name("ft_protocol: happy path wire sequence META/DATA/FINAL/ABORT");
    unity_run_test_by_name("ft_data_pipe: add find clear and capacity");

    UNITY_END();
}
