/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <inttypes.h>
#include <stdlib.h>
#include <string.h>

#include "unity.h"
#include "unity_test_utils_memory.h"

#include "esp_check.h"
#include "esp_http_client.h"
#include "esp_http_server.h"
#include "esp_log.h"
#include "esp_random.h"

#include "protocomm.h"
#include "protocomm_security.h"
#include "protocomm_security1.h"
#include "protocomm_ext.h"
#include "protocomm_ext_security.h"
#include "esp_local_controller.h"
#include "esp_local_ctrl.pb-c.h"

static const char *TAG = "lc_dual_peer";

#define NO_SESSION_ID UINT32_MAX
#define MAX_BODY      4096

extern void esp_local_controller_test_set_leak_threshold(int bytes);

static const char s_pop[] = "abcd1234";
static const char s_version_json[] =
    "{\"local_ctrl\":{\"ver\":\"v1.0\",\"sec_ver\":1,\"sec_patch_ver\":0}}";
static const protocomm_security1_params_t s_server_pop = {
    .data = (const uint8_t *)s_pop,
    .len = sizeof(s_pop) - 1,
};
static const protocomm_ext_security1_params_t s_client_pop = {
    .data = (const uint8_t *)s_pop,
    .len = sizeof(s_pop) - 1,
};

typedef struct {
    protocomm_t *pc;
    httpd_handle_t hd;
    uint16_t port;
    uint32_t prop_count;
    uint32_t cookie_id;
    int sock_id;
    int session_resets;
    int session_posts;
    int control_posts;
    char issued_cookie[24];
    bool control_cookie_ok;
} peer_srv_t;

typedef struct {
    char url[48];
    esp_http_client_config_t http;
    protocomm_ext_t *pc;
    esp_local_controller_t *ctrl;
} peer_cli_t;

static esp_err_t control_handler(uint32_t session_id, const uint8_t *inbuf, ssize_t inlen,
                                 uint8_t **outbuf, ssize_t *outlen, void *priv_data)
{
    peer_srv_t *srv = priv_data;
    (void)session_id;
    if (!srv || inlen <= 0) {
        return ESP_FAIL;
    }
    LocalCtrlMessage *req = local_ctrl_message__unpack(NULL, (size_t)inlen, inbuf);
    if (!req || req->msg != LOCAL_CTRL_MSG_TYPE__TypeCmdGetPropertyCount) {
        local_ctrl_message__free_unpacked(req, NULL);
        return ESP_FAIL;
    }
    local_ctrl_message__free_unpacked(req, NULL);

    LocalCtrlMessage msg = LOCAL_CTRL_MESSAGE__INIT;
    RespGetPropertyCount resp = RESP_GET_PROPERTY_COUNT__INIT;
    resp.status = STATUS__Success;
    resp.count = srv->prop_count;
    msg.msg = LOCAL_CTRL_MSG_TYPE__TypeRespGetPropertyCount;
    msg.payload_case = LOCAL_CTRL_MESSAGE__PAYLOAD_RESP_GET_PROP_COUNT;
    msg.resp_get_prop_count = &resp;

    size_t len = local_ctrl_message__get_packed_size(&msg);
    uint8_t *buf = malloc(len);
    if (!buf) {
        return ESP_ERR_NO_MEM;
    }
    local_ctrl_message__pack(&msg, buf);
    *outbuf = buf;
    *outlen = (ssize_t)len;
    srv->control_posts++;
    return ESP_OK;
}

static esp_err_t peer_post(httpd_req_t *req)
{
    peer_srv_t *srv = req->user_ctx;
    if (!srv || !srv->pc) {
        return ESP_FAIL;
    }

    int sock = httpd_req_to_sockfd(req);
    char cookie_hdr[32] = {0};
    bool got_cookie = httpd_req_get_hdr_value_str(req, "Cookie", cookie_hdr, sizeof(cookie_hdr)) == ESP_OK;
    bool same = false;
    if (got_cookie) {
        same = srv->issued_cookie[0] && strcmp(cookie_hdr, srv->issued_cookie) == 0;
    } else if (srv->sock_id >= 0 && sock == srv->sock_id) {
        same = true;
    }

    if (!same) {
        if (srv->cookie_id != NO_SESSION_ID) {
            protocomm_close_session(srv->pc, srv->cookie_id);
            srv->session_resets++;
            srv->cookie_id = NO_SESSION_ID;
        }
        uint32_t id;
        do {
            id = esp_random();
        } while (id == NO_SESSION_ID);
        if (protocomm_open_session(srv->pc, id) != ESP_OK) {
            return ESP_FAIL;
        }
        srv->cookie_id = id;
        srv->sock_id = sock;
        snprintf(srv->issued_cookie, sizeof(srv->issued_cookie), "session=%" PRIu32, id);
        httpd_resp_set_hdr(req, "Set-Cookie", srv->issued_cookie);
    }

    if (req->content_len <= 0 || req->content_len > MAX_BODY) {
        return ESP_FAIL;
    }
    uint8_t *body = malloc(req->content_len);
    if (!body) {
        return ESP_ERR_NO_MEM;
    }
    int received = 0;
    while (received < req->content_len) {
        int n = httpd_req_recv(req, (char *)body + received, req->content_len - received);
        if (n <= 0) {
            free(body);
            return ESP_FAIL;
        }
        received += n;
    }

    const char *ep = req->uri[0] == '/' ? req->uri + 1 : req->uri;
    if (strcmp(ep, ESP_LOCAL_CONTROLLER_EP_SESSION) == 0) {
        srv->session_posts++;
    }
    if (strcmp(ep, ESP_LOCAL_CONTROLLER_EP_CONTROL) == 0) {
        srv->control_cookie_ok = got_cookie && strcmp(cookie_hdr, srv->issued_cookie) == 0;
    }

    uint8_t *out = NULL;
    ssize_t outlen = 0;
    esp_err_t err = protocomm_req_handle(srv->pc, ep, srv->cookie_id, body, received, &out, &outlen);
    free(body);
    if (err != ESP_OK) {
        free(out);
        ESP_LOGE(TAG, "req %s failed: %s", ep, esp_err_to_name(err));
        return ESP_FAIL;
    }
    err = httpd_resp_send(req, (const char *)out, outlen);
    free(out);
    return err;
}

static void peer_stop(peer_srv_t *srv)
{
    if (!srv) {
        return;
    }
    if (srv->hd) {
        httpd_stop(srv->hd);
        srv->hd = NULL;
    }
    if (srv->pc) {
        protocomm_delete(srv->pc);
        srv->pc = NULL;
    }
}

static esp_err_t peer_start(peer_srv_t *srv, uint16_t port, uint32_t prop_count)
{
    memset(srv, 0, sizeof(*srv));
    srv->port = port;
    srv->prop_count = prop_count;
    srv->cookie_id = NO_SESSION_ID;
    srv->sock_id = -1;

    srv->pc = protocomm_new();
    if (!srv->pc) {
        return ESP_ERR_NO_MEM;
    }
    esp_err_t err = protocomm_set_version(srv->pc, ESP_LOCAL_CONTROLLER_EP_VERSION, s_version_json);
    if (err != ESP_OK) {
        peer_stop(srv);
        return err;
    }
    err = protocomm_set_security(srv->pc, ESP_LOCAL_CONTROLLER_EP_SESSION, &protocomm_security1, &s_server_pop);
    if (err != ESP_OK) {
        peer_stop(srv);
        return err;
    }
    err = protocomm_add_endpoint(srv->pc, ESP_LOCAL_CONTROLLER_EP_CONTROL, control_handler, srv);
    if (err != ESP_OK) {
        peer_stop(srv);
        return err;
    }

    httpd_config_t cfg = HTTPD_DEFAULT_CONFIG();
    cfg.server_port = port;
    cfg.ctrl_port = (uint16_t)(port + 1000);
    cfg.stack_size = 12288;
    cfg.lru_purge_enable = true;
    cfg.max_open_sockets = 4;
    cfg.keep_alive_enable = true;
    cfg.keep_alive_idle = 5;
    cfg.keep_alive_interval = 5;
    cfg.keep_alive_count = 3;

    err = httpd_start(&srv->hd, &cfg);
    if (err != ESP_OK) {
        peer_stop(srv);
        return err;
    }
    static const char *uris[] = {
        "/" ESP_LOCAL_CONTROLLER_EP_VERSION,
        "/" ESP_LOCAL_CONTROLLER_EP_SESSION,
        "/" ESP_LOCAL_CONTROLLER_EP_CONTROL,
    };
    for (size_t i = 0; i < sizeof(uris) / sizeof(uris[0]); i++) {
        httpd_uri_t uri = {
            .uri = uris[i],
            .method = HTTP_POST,
            .handler = peer_post,
            .user_ctx = srv,
        };
        err = httpd_register_uri_handler(srv->hd, &uri);
        if (err != ESP_OK) {
            peer_stop(srv);
            return err;
        }
    }
    return ESP_OK;
}

static void cli_stop(peer_cli_t *cli)
{
    if (!cli) {
        return;
    }
    if (cli->ctrl) {
        esp_local_controller_stop_session(cli->ctrl);
        esp_local_controller_delete(cli->ctrl);
        cli->ctrl = NULL;
    }
    if (cli->pc) {
        protocomm_ext_delete(cli->pc);
        cli->pc = NULL;
    }
}

static esp_err_t cli_open(peer_cli_t *cli, uint16_t port)
{
    memset(cli, 0, sizeof(*cli));
    snprintf(cli->url, sizeof(cli->url), "http://127.0.0.1:%u", (unsigned)port);
    cli->http.url = cli->url;
    cli->http.timeout_ms = 8000;
    protocomm_ext_config_data_t cfg = {
        .transport_method = PROTOCOMM_EXT_TRANSPORT_METHOD_HTTP,
        .security_method = PROTOCOMM_EXT_SECURITY_METHOD_NONE,
        .transport_data = &cli->http,
        .security_data = NULL,
    };
    cli->pc = protocomm_ext_new(&cfg);
    if (!cli->pc) {
        return ESP_ERR_NO_MEM;
    }
    cli->ctrl = esp_local_controller_create(cli->pc);
    if (!cli->ctrl) {
        return ESP_ERR_NO_MEM;
    }
    esp_err_t err = esp_local_controller_fetch_version(cli->ctrl, NULL);
    if (err != ESP_OK) {
        return err;
    }
    esp_local_controller_version_t ver = {0};
    err = esp_local_controller_get_version(cli->ctrl, &ver);
    if (err != ESP_OK) {
        return err;
    }
    bool sec1 = ver.sec_ver == 1;
    esp_local_controller_version_free(&ver);
    if (!sec1) {
        return ESP_ERR_INVALID_RESPONSE;
    }
    err = protocomm_ext_set_security(cli->pc, PROTOCOMM_EXT_SECURITY_METHOD_SECURITY_1, &s_client_pop);
    if (err != ESP_OK) {
        return err;
    }
    return esp_local_controller_establish_security(cli->ctrl);
}

static esp_err_t cli_count(peer_cli_t *cli, uint32_t *count)
{
    return esp_local_controller_get_property_count(cli->ctrl, count);
}

TEST_CASE("two HTTP peers round-trip properties on loopback", "[esp_local_controller]")
{
    esp_local_controller_test_set_leak_threshold(8192);

    peer_srv_t srv_a = {0};
    peer_srv_t srv_b = {0};
    peer_cli_t cli_a = {0};
    peer_cli_t cli_b = {0};
    TEST_ASSERT_EQUAL(ESP_OK, peer_start(&srv_a, 18091, 11));
    TEST_ASSERT_EQUAL(ESP_OK, peer_start(&srv_b, 18092, 22));

    TEST_ASSERT_EQUAL(ESP_OK, cli_open(&cli_a, 18091));
    uint32_t count = 0;
    TEST_ASSERT_EQUAL(ESP_OK, cli_count(&cli_a, &count));
    TEST_ASSERT_EQUAL_UINT32(11, count);
    int session_posts_a = srv_a.session_posts;

    TEST_ASSERT_EQUAL(ESP_OK, cli_open(&cli_b, 18092));
    TEST_ASSERT_EQUAL(ESP_OK, cli_count(&cli_b, &count));
    TEST_ASSERT_EQUAL_UINT32(22, count);

    TEST_ASSERT_EQUAL(ESP_OK, cli_count(&cli_a, &count));
    TEST_ASSERT_EQUAL_UINT32(11, count);
    TEST_ASSERT_EQUAL_INT(session_posts_a, srv_a.session_posts);
    TEST_ASSERT_EQUAL_INT(0, srv_a.session_resets);
    TEST_ASSERT_EQUAL_INT(0, srv_b.session_resets);
    TEST_ASSERT_TRUE(srv_a.control_cookie_ok);
    TEST_ASSERT_TRUE(srv_b.control_cookie_ok);
    TEST_ASSERT_TRUE(srv_a.issued_cookie[0] != '\0');
    TEST_ASSERT_NOT_EQUAL(0, strcmp(srv_a.issued_cookie, srv_b.issued_cookie));

    int session_posts_b = srv_b.session_posts;
    int control_posts_b = srv_b.control_posts;
    cli_stop(&cli_a);
    TEST_ASSERT_EQUAL(ESP_OK, cli_count(&cli_b, &count));
    TEST_ASSERT_EQUAL_UINT32(22, count);
    TEST_ASSERT_EQUAL_INT(session_posts_b, srv_b.session_posts);
    TEST_ASSERT_EQUAL_INT(control_posts_b + 1, srv_b.control_posts);
    TEST_ASSERT_EQUAL_INT(0, srv_b.session_resets);

    cli_stop(&cli_b);
    peer_stop(&srv_a);
    peer_stop(&srv_b);
}
