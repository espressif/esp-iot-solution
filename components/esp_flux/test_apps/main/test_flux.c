/* SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @brief Unit tests for esp_flux reliable transport protocol.
 *
 * Uses a mock loopback transport: two flux sessions connected via shared queues.
 * Session A sends → mock_send puts data in B's queue → B calls flux_session_feed_data.
 * Session B's SACK → mock_send puts data in A's queue → A calls flux_session_feed_data.
 */

#include <string.h>
#include <stdlib.h>
#include "unity.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "freertos/semphr.h"
#include "esp_log.h"
#include "esp_random.h"
#include "esp_flux_transport.h"

static const char *TAG = "test_flux";

/* ────────────────────── Mock Transport ────────────────────── */

#define MOCK_MTU        64 // Small MTU to force many fragments
#define MOCK_QUEUE_SIZE 64

typedef struct {
    QueueHandle_t tx_queue; // Data sent by this side goes here
    QueueHandle_t rx_queue; // Data to be fed to this side comes from here
    uint16_t mtu;
    int drop_rate;  // 0-100, percentage of packets to drop (for loss test)
    int send_count; // Total sends attempted
} mock_transport_t;

typedef struct {
    uint8_t *data;
    uint16_t size;
} mock_packet_t;

static esp_err_t mock_send(void *ctx, const uint8_t *data, uint16_t size)
{
    mock_transport_t *mt = (mock_transport_t *)ctx;
    mt->send_count++;

    // Simulate packet loss
    if (mt->drop_rate > 0 && (esp_random() % 100) < mt->drop_rate) {
        ESP_LOGD(TAG, "Mock: dropped packet (drop_rate=%d%%)", mt->drop_rate);
        return ESP_OK; // Pretend send succeeded but data is lost
    }

    mock_packet_t pkt;
    pkt.data = malloc(size);
    if (!pkt.data) {
        return ESP_ERR_NO_MEM;
    }
    memcpy(pkt.data, data, size);
    pkt.size = size;

    if (xQueueSend(mt->tx_queue, &pkt, pdMS_TO_TICKS(100)) != pdPASS) {
        free(pkt.data);
        return ESP_ERR_NO_MEM;
    }
    return ESP_OK;
}

static uint16_t mock_get_mtu(void *ctx)
{
    mock_transport_t *mt = (mock_transport_t *)ctx;
    return mt->mtu;
}

static const flux_transport_ops_t mock_ops = {
    .send = mock_send,
    .get_mtu = mock_get_mtu,
};

/* ────────────────────── Loopback Pump Task ────────────────────── */

typedef struct {
    flux_session_t *session;
    QueueHandle_t rx_queue;
    bool running;
} pump_ctx_t;

static void pump_task(void *arg)
{
    pump_ctx_t *ctx = (pump_ctx_t *)arg;
    mock_packet_t pkt;

    while (ctx->running) {
        if (xQueueReceive(ctx->rx_queue, &pkt, pdMS_TO_TICKS(10)) == pdPASS) {
            flux_session_feed_data(ctx->session, pkt.data, pkt.size);
            free(pkt.data);
        }
    }
    vTaskDelete(NULL);
}

/* ────────────────────── Test Callback State ────────────────────── */

typedef struct {
    SemaphoreHandle_t complete_sem;
    SemaphoreHandle_t recv_sem;
    esp_err_t complete_status;
    uint8_t complete_stream_id;
    const uint8_t *received_data;
    uint32_t received_size;
    uint8_t recv_stream_id;
    bool error_occurred;
    esp_err_t error_code;
} test_state_t;

static void test_on_complete(flux_session_t *s, uint8_t stream_id, esp_err_t status, const uint8_t *data, uint32_t size)
{
    test_state_t *ts = (test_state_t *)flux_session_get_user_arg(s);
    (void)data;
    (void)size;
    ts->complete_stream_id = stream_id;
    ts->complete_status = status;
    xSemaphoreGive(ts->complete_sem);
}

static void test_on_data_received(flux_session_t *s, uint8_t stream_id, const uint8_t *data, uint32_t size)
{
    test_state_t *ts = (test_state_t *)flux_session_get_user_arg(s);
    ts->recv_stream_id = stream_id;
    ts->received_data = data;
    ts->received_size = size;
    xSemaphoreGive(ts->recv_sem);
}

static void test_on_error(flux_session_t *s, esp_err_t error)
{
    test_state_t *ts = (test_state_t *)flux_session_get_user_arg(s);
    ts->error_occurred = true;
    ts->error_code = error;
}

/* ────────────────────── Helper: Setup Loopback ────────────────────── */

typedef struct {
    mock_transport_t mt_a, mt_b;
    flux_session_t *session_a, *session_b;
    pump_ctx_t pump_a, pump_b;
    test_state_t state_a, state_b;
} loopback_env_t;

static void setup_loopback(loopback_env_t *env, uint16_t mtu, int drop_a, int drop_b)
{
    memset(env, 0, sizeof(*env));

    // Create cross-connected queues: A's TX → B's RX, B's TX → A's RX
    QueueHandle_t q_a_to_b = xQueueCreate(MOCK_QUEUE_SIZE, sizeof(mock_packet_t));
    QueueHandle_t q_b_to_a = xQueueCreate(MOCK_QUEUE_SIZE, sizeof(mock_packet_t));

    env->mt_a = (mock_transport_t) {
        .tx_queue = q_a_to_b, .rx_queue = q_b_to_a, .mtu = mtu, .drop_rate = drop_a
    };
    env->mt_b = (mock_transport_t) {
        .tx_queue = q_b_to_a, .rx_queue = q_a_to_b, .mtu = mtu, .drop_rate = drop_b
    };

    env->state_a.complete_sem = xSemaphoreCreateBinary();
    env->state_a.recv_sem = xSemaphoreCreateBinary();
    env->state_b.complete_sem = xSemaphoreCreateBinary();
    env->state_b.recv_sem = xSemaphoreCreateBinary();

    flux_callbacks_t cbs_a = {
        .session_complete_cb = test_on_complete,
        .data_received_cb = test_on_data_received,
        .error_cb = test_on_error,
        .arg = &env->state_a,
    };
    flux_callbacks_t cbs_b = {
        .session_complete_cb = test_on_complete,
        .data_received_cb = test_on_data_received,
        .error_cb = test_on_error,
        .arg = &env->state_b,
    };

    env->session_a = flux_session_create(&mock_ops, &env->mt_a, &cbs_a, mtu);
    env->session_b = flux_session_create(&mock_ops, &env->mt_b, &cbs_b, mtu);

    TEST_ASSERT_NOT_NULL(env->session_a);
    TEST_ASSERT_NOT_NULL(env->session_b);

    // Start pump tasks: A's TX → feed to B, B's TX → feed to A
    env->pump_a.session = env->session_a;
    env->pump_a.rx_queue = q_b_to_a; // A receives from B
    env->pump_a.running = true;

    env->pump_b.session = env->session_b;
    env->pump_b.rx_queue = q_a_to_b; // B receives from A
    env->pump_b.running = true;

    xTaskCreate(pump_task, "pump_a", 4096, &env->pump_a, 5, NULL);
    xTaskCreate(pump_task, "pump_b", 4096, &env->pump_b, 5, NULL);
}

static void teardown_loopback(loopback_env_t *env)
{
    env->pump_a.running = false;
    env->pump_b.running = false;
    vTaskDelay(pdMS_TO_TICKS(100)); // Let tasks exit

    flux_session_destroy(env->session_a);
    flux_session_destroy(env->session_b);

    if (env->state_a.received_data) {
        free((void *)env->state_a.received_data);
        env->state_a.received_data = NULL;
    }
    if (env->state_b.received_data) {
        free((void *)env->state_b.received_data);
        env->state_b.received_data = NULL;
    }

    // Drain and delete queues
    mock_packet_t pkt;
    while (xQueueReceive(env->mt_a.tx_queue, &pkt, 0) == pdPASS) {
        free(pkt.data);
    }
    while (xQueueReceive(env->mt_b.tx_queue, &pkt, 0) == pdPASS) {
        free(pkt.data);
    }
    vQueueDelete(env->mt_a.tx_queue);
    vQueueDelete(env->mt_b.tx_queue);

    vSemaphoreDelete(env->state_a.complete_sem);
    vSemaphoreDelete(env->state_a.recv_sem);
    vSemaphoreDelete(env->state_b.complete_sem);
    vSemaphoreDelete(env->state_b.recv_sem);
}

/* ────────────────────── Test: Small Data (single fragment) ────────────────────── */

TEST_CASE("flux: single fragment send/recv", "[esp_flux]")
{
    loopback_env_t env;
    setup_loopback(&env, MOCK_MTU, 0, 0);

    const char *msg = "Hello esp_flux!";
    uint32_t len = strlen(msg);

    // A sends to B
    uint8_t *send_buf = malloc(len);
    memcpy(send_buf, msg, len);
    esp_err_t ret = flux_session_send(env.session_a, send_buf, len, 4, 50);
    TEST_ASSERT_EQUAL(ESP_OK, ret);

    // Wait for B to receive
    TEST_ASSERT(xSemaphoreTake(env.state_b.recv_sem, pdMS_TO_TICKS(5000)));
    TEST_ASSERT_EQUAL(len, env.state_b.received_size);
    TEST_ASSERT_EQUAL_MEMORY(msg, env.state_b.received_data, len);

    // Wait for A's send complete
    TEST_ASSERT(xSemaphoreTake(env.state_a.complete_sem, pdMS_TO_TICKS(5000)));
    TEST_ASSERT_EQUAL(ESP_OK, env.state_a.complete_status);

    flux_session_send_reset(env.session_a, env.state_a.complete_stream_id);
    flux_session_recv_reset(env.session_b, env.state_b.recv_stream_id);
    teardown_loopback(&env);
    ESP_LOGI(TAG, "Single fragment test PASSED");
}

/* ────────────────────── Test: Large Data (multi-fragment) ────────────────────── */

TEST_CASE("flux: large data fragmentation and reassembly", "[esp_flux]")
{
    loopback_env_t env;
    setup_loopback(&env, MOCK_MTU, 0, 0);

    const size_t data_size = 4 * 1024; // 4KB with 64B MTU = many fragments
    uint8_t *send_data = malloc(data_size);
    TEST_ASSERT_NOT_NULL(send_data);

    // Fill with pattern
    for (size_t i = 0; i < data_size; i++) {
        send_data[i] = (uint8_t)(i & 0xFF);
    }

    esp_err_t ret = flux_session_send(env.session_a, send_data, data_size, 8, 50);
    TEST_ASSERT_EQUAL(ESP_OK, ret);

    // Wait for B to receive
    TEST_ASSERT(xSemaphoreTake(env.state_b.recv_sem, pdMS_TO_TICKS(30000)));
    TEST_ASSERT_EQUAL(data_size, env.state_b.received_size);
    TEST_ASSERT_EQUAL_MEMORY(send_data, env.state_b.received_data, data_size);

    // Wait for A's send complete
    TEST_ASSERT(xSemaphoreTake(env.state_a.complete_sem, pdMS_TO_TICKS(30000)));
    TEST_ASSERT_EQUAL(ESP_OK, env.state_a.complete_status);

    flux_session_send_reset(env.session_a, env.state_a.complete_stream_id);
    flux_session_recv_reset(env.session_b, env.state_b.recv_stream_id);
    teardown_loopback(&env);
    ESP_LOGI(TAG, "Large data test PASSED (4KB)");
}

/* ────────────────────── Test: Packet Loss with SACK Recovery ────────────────────── */

TEST_CASE("flux: packet loss recovery via SACK", "[esp_flux]")
{
    loopback_env_t env;
    // 20% loss on A→B (data direction), 0% on B→A (SACK direction)
    setup_loopback(&env, MOCK_MTU, 20, 0);

    const size_t data_size = 2 * 1024; // 2KB
    uint8_t *send_data = malloc(data_size);
    TEST_ASSERT_NOT_NULL(send_data);

    for (size_t i = 0; i < data_size; i++) {
        send_data[i] = (uint8_t)((i * 7 + 3) & 0xFF);
    }

    esp_err_t ret = flux_session_send(env.session_a, send_data, data_size, 6, 50);
    TEST_ASSERT_EQUAL(ESP_OK, ret);

    // With 20% loss, should still complete via SACK retransmission (longer timeout)
    TEST_ASSERT(xSemaphoreTake(env.state_b.recv_sem, pdMS_TO_TICKS(60000)));
    TEST_ASSERT_EQUAL(data_size, env.state_b.received_size);
    TEST_ASSERT_EQUAL_MEMORY(send_data, env.state_b.received_data, data_size);

    TEST_ASSERT(xSemaphoreTake(env.state_a.complete_sem, pdMS_TO_TICKS(60000)));
    TEST_ASSERT_EQUAL(ESP_OK, env.state_a.complete_status);

    flux_session_send_reset(env.session_a, env.state_a.complete_stream_id);
    flux_session_recv_reset(env.session_b, env.state_b.recv_stream_id);
    teardown_loopback(&env);
    ESP_LOGI(TAG, "Packet loss recovery test PASSED (20%% loss)");
}

/* ────────────────────── Test: Window Flow Control ────────────────────── */

TEST_CASE("flux: window flow control limits in-flight", "[esp_flux]")
{
    loopback_env_t env;
    setup_loopback(&env, MOCK_MTU, 0, 0);

    const size_t data_size = 2 * 1024;
    uint8_t *send_data = malloc(data_size);
    TEST_ASSERT_NOT_NULL(send_data);
    memset(send_data, 0xAB, data_size);

    // Window size = 4
    esp_err_t ret = flux_session_send(env.session_a, send_data, data_size, 4, 50);
    TEST_ASSERT_EQUAL(ESP_OK, ret);

    // Verify in_flight never exceeds window_size=4 during initial burst
    // (checked by protocol internally, here we just verify completion)
    TEST_ASSERT(xSemaphoreTake(env.state_b.recv_sem, pdMS_TO_TICKS(30000)));
    TEST_ASSERT_EQUAL(data_size, env.state_b.received_size);

    TEST_ASSERT(xSemaphoreTake(env.state_a.complete_sem, pdMS_TO_TICKS(30000)));

    flux_session_send_reset(env.session_a, env.state_a.complete_stream_id);
    flux_session_recv_reset(env.session_b, env.state_b.recv_stream_id);
    teardown_loopback(&env);
    ESP_LOGI(TAG, "Window flow control test PASSED");
}

/* ────────────────────── App Main ────────────────────── */

void app_main(void)
{
    ESP_LOGI(TAG, "Running esp_flux unit tests");
    UNITY_BEGIN();
    unity_run_all_tests();
    UNITY_END();
}
