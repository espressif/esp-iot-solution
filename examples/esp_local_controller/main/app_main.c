/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Interactive controller example for IDF esp_local_ctrl.
 * Transports: HTTP / BLE / Console. Security: runtime Sec0/1/2 from version JSON.
 */

#include <stdio.h>
#include <string.h>
#include <strings.h>
#include <stdlib.h>
#include <inttypes.h>

#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "freertos/task.h"

#include "esp_check.h"
#include "esp_event.h"
#include "esp_log.h"
#include "esp_wifi.h"
#include "esp_http_client.h"
#include "nvs_flash.h"
#include "sdkconfig.h"

#include "protocomm_ext.h"
#include "protocomm_ext_security.h"
#include "esp_local_controller.h"

#if CONFIG_EXAMPLE_SUPPORT_BLE
#include "protocomm_ext_nimble.h"
#include "host/ble_gap.h"
#endif

#if CONFIG_EXAMPLE_SUPPORT_CONSOLE
#include "driver/uart.h"
#include "protocomm_ext_console.h"
#endif

static const char *TAG = "lc_example";

/* Match IDF esp_local_ctrl example property types / flags. */
enum {
    PROP_TYPE_TIMESTAMP = 0,
    PROP_TYPE_INT32,
    PROP_TYPE_BOOLEAN,
    PROP_TYPE_STRING,
};
#define PROP_FLAG_READONLY (1U << 0)

typedef enum {
    MODE_HTTP = 1,
    MODE_BLE = 2,
    MODE_CONSOLE = 3,
} ctrl_mode_t;

static int read_console_int(const char *prompt)
{
    printf("%s", prompt);
    fflush(stdout);

    char line[32];
    int len = 0;
    while (1) {
        int c = getchar();
        if (c == EOF) {
            clearerr(stdin);
            vTaskDelay(pdMS_TO_TICKS(20));
            continue;
        }
        if (c == '\r' || c == '\n') {
            if (len == 0) {
                continue;
            }
            line[len] = '\0';
            return atoi(line);
        }
        if (len < (int)sizeof(line) - 1) {
            line[len++] = (char)c;
        }
    }
}

static esp_err_t read_console_line_default(const char *prompt, char *buf, size_t buf_len,
                                           const char *default_val, bool allow_empty, bool echo)
{
    if (!buf || buf_len < 2) {
        return ESP_ERR_INVALID_ARG;
    }
    printf("%s", prompt);
    fflush(stdout);

    size_t len = 0;
    while (1) {
        int c = getchar();
        if (c == EOF) {
            clearerr(stdin);
            vTaskDelay(pdMS_TO_TICKS(20));
            continue;
        }
        if (c == '\r' || c == '\n') {
            if (len == 0) {
                if (default_val && default_val[0]) {
                    strncpy(buf, default_val, buf_len - 1);
                    buf[buf_len - 1] = '\0';
                    printf("(default)\n");
                    return ESP_OK;
                }
                if (!allow_empty) {
                    continue;
                }
            }
            buf[len] = '\0';
            printf("\n");
            return ESP_OK;
        }
        if (c == 0x08 || c == 0x7f) {
            if (len > 0) {
                len--;
                printf("\b \b");
                fflush(stdout);
            }
            continue;
        }
        if (c >= 32 && c < 127 && len + 1 < buf_len) {
            buf[len++] = (char)c;
            putchar(echo ? c : '*');
            fflush(stdout);
        }
    }
}

static esp_err_t read_console_secret(const char *prompt, char *buf, size_t buf_len,
                                     const char *default_val, bool allow_empty)
{
    return read_console_line_default(prompt, buf, buf_len, default_val, allow_empty, false);
}

static char s_sec1_pop[64];
static char s_sec2_user[64];
static char s_sec2_pass[64];
static protocomm_ext_security1_params_t s_sec1_params;
static protocomm_ext_security2_params_t s_sec2_params;

static esp_err_t configure_security_from_version(protocomm_ext_t *pc,
                                                 const esp_local_controller_version_t *ver)
{
    int sec_ver = ver->sec_ver;
    if (sec_ver < 0 || sec_ver > 2) {
        ESP_LOGE(TAG, "Unsupported sec_ver=%d", sec_ver);
        return ESP_ERR_NOT_SUPPORTED;
    }

    printf("\nPeer security: Sec%d (patch=%d)\n", sec_ver, ver->sec_patch_ver);

    const void *sec_data = NULL;
    protocomm_ext_security_method_t method = PROTOCOMM_EXT_SECURITY_METHOD_NONE;

    if (sec_ver == 0) {
        printf("Sec0: no credentials required.\n");
    } else if (sec_ver == 1) {
        method = PROTOCOMM_EXT_SECURITY_METHOD_SECURITY_1;
        ESP_RETURN_ON_ERROR(read_console_secret("Enter Proof of Possession (Enter=default): ",
                                                s_sec1_pop, sizeof(s_sec1_pop),
                                                CONFIG_EXAMPLE_CTRL_POP, false),
                            TAG, "read pop");
        s_sec1_params.data = (const uint8_t *)s_sec1_pop;
        s_sec1_params.len = strlen(s_sec1_pop);
        sec_data = &s_sec1_params;
    } else {
        method = PROTOCOMM_EXT_SECURITY_METHOD_SECURITY_2;
        ESP_RETURN_ON_ERROR(read_console_line_default(
                                "Enter Sec2 username (Enter=default): ",
                                s_sec2_user, sizeof(s_sec2_user),
                                CONFIG_EXAMPLE_CTRL_SEC2_USERNAME, false, true),
                            TAG, "read user");
        ESP_RETURN_ON_ERROR(read_console_secret("Enter Sec2 password (Enter=default): ",
                                                s_sec2_pass, sizeof(s_sec2_pass),
                                                CONFIG_EXAMPLE_CTRL_SEC2_PASSWORD, false),
                            TAG, "read pass");
        s_sec2_params.username = s_sec2_user;
        s_sec2_params.username_len = strlen(s_sec2_user);
        s_sec2_params.password = s_sec2_pass;
        s_sec2_params.password_len = strlen(s_sec2_pass);
        sec_data = &s_sec2_params;
    }

    return protocomm_ext_set_security(pc, method, sec_data);
}

static void clear_security_credentials(void)
{
    memset(s_sec1_pop, 0, sizeof(s_sec1_pop));
    memset(s_sec2_user, 0, sizeof(s_sec2_user));
    memset(s_sec2_pass, 0, sizeof(s_sec2_pass));
    memset(&s_sec1_params, 0, sizeof(s_sec1_params));
    memset(&s_sec2_params, 0, sizeof(s_sec2_params));
}

#if CONFIG_EXAMPLE_SUPPORT_HTTP
#define WIFI_CONNECTED_BIT BIT0
#define WIFI_FAIL_BIT      BIT1

static EventGroupHandle_t s_wifi_events;
static int s_wifi_retry;

static void wifi_event_handler(void *arg, esp_event_base_t base, int32_t id, void *data)
{
    if (base == WIFI_EVENT && id == WIFI_EVENT_STA_START) {
        esp_wifi_connect();
    } else if (base == WIFI_EVENT && id == WIFI_EVENT_STA_DISCONNECTED) {
        if (s_wifi_retry < 5) {
            esp_wifi_connect();
            s_wifi_retry++;
        } else {
            xEventGroupSetBits(s_wifi_events, WIFI_FAIL_BIT);
        }
    } else if (base == IP_EVENT && id == IP_EVENT_STA_GOT_IP) {
        s_wifi_retry = 0;
        xEventGroupSetBits(s_wifi_events, WIFI_CONNECTED_BIT);
    }
}

static esp_err_t wifi_sta_connect(void)
{
    s_wifi_events = xEventGroupCreate();
    ESP_RETURN_ON_FALSE(s_wifi_events, ESP_ERR_NO_MEM, TAG, "event group");

    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_create_default_wifi_sta();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));
    ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &wifi_event_handler, NULL));
    ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &wifi_event_handler, NULL));

    wifi_config_t wifi_config = {0};
    strncpy((char *)wifi_config.sta.ssid, CONFIG_EXAMPLE_WIFI_SSID, sizeof(wifi_config.sta.ssid) - 1);
    strncpy((char *)wifi_config.sta.password, CONFIG_EXAMPLE_WIFI_PASSWORD,
            sizeof(wifi_config.sta.password) - 1);

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());

    EventBits_t bits = xEventGroupWaitBits(s_wifi_events, WIFI_CONNECTED_BIT | WIFI_FAIL_BIT,
                                           pdFALSE, pdFALSE, pdMS_TO_TICKS(20000));
    if (bits & WIFI_CONNECTED_BIT) {
        ESP_LOGI(TAG, "STA connected to %s", CONFIG_EXAMPLE_WIFI_SSID);
        return ESP_OK;
    }
    ESP_LOGE(TAG, "STA failed to join %s", CONFIG_EXAMPLE_WIFI_SSID);
    return ESP_FAIL;
}
#endif /* HTTP */

#if CONFIG_EXAMPLE_SUPPORT_BLE
#define BLE_SCAN_SELECT_MAX 32

typedef struct {
    ble_addr_t addr;
    char name[32];
    bool match;
} ble_peer_choice_t;

static esp_err_t ble_pick_device(protocomm_ext_t *pc, ble_addr_t *out_addr)
{
    void *th = protocomm_ext_get_transport_handle(pc);
    ESP_RETURN_ON_FALSE(th, ESP_ERR_INVALID_STATE, TAG, "no transport handle");

    protocomm_ext_nimble_scan_config_t scan_cfg = {
        .scan_timeout_ms = 8000,
    };
    ESP_RETURN_ON_ERROR(esp_protocomm_ext_nimble_start_scan(th, &scan_cfg), TAG, "scan start");
    esp_protocomm_ext_nimble_stop_scan(th);

    int count = esp_protocomm_ext_nimble_get_scanned_device_count(th);
    if (count <= 0) {
        ESP_LOGW(TAG, "No BLE devices found");
        return ESP_ERR_NOT_FOUND;
    }

    ble_peer_choice_t choices[BLE_SCAN_SELECT_MAX];
    int nchoice = 0;
    const size_t prefix_len = strlen(CONFIG_EXAMPLE_BLE_NAME_PREFIX);

    /* Pass 1: prefix matches first so busy RF environments do not hide the peer. */
    for (int pass = 0; pass < 2 && nchoice < BLE_SCAN_SELECT_MAX; pass++) {
        for (int i = 0; i < count && nchoice < BLE_SCAN_SELECT_MAX; i++) {
            protocomm_ext_nimble_scanned_device_info_t info = {0};
            if (esp_protocomm_ext_nimble_get_scanned_device_info(th, i, &info) != ESP_OK) {
                continue;
            }
            bool match = info.name &&
                         strncmp((const char *)info.name, CONFIG_EXAMPLE_BLE_NAME_PREFIX, prefix_len) == 0;
            if ((pass == 0 && !match) || (pass == 1 && match)) {
                free(info.name);
                free(info.mfg_data);
                continue;
            }
            const char *name = info.name ? (const char *)info.name : "(no name)";
            choices[nchoice].addr = info.addr;
            choices[nchoice].match = match;
            memset(choices[nchoice].name, 0, sizeof(choices[nchoice].name));
            strncpy(choices[nchoice].name, name, sizeof(choices[nchoice].name) - 1);
            nchoice++;
            free(info.name);
            free(info.mfg_data);
        }
    }
    if (nchoice == 0) {
        return ESP_ERR_NOT_FOUND;
    }

    printf("\nBLE scan results:\n");
    for (int i = 0; i < nchoice; i++) {
        printf("  %d) %s%s\n", i + 1, choices[i].name, choices[i].match ? "  <match>" : "");
    }

    int choice = read_console_int("Select BLE device (0=cancel): ");
    if (choice <= 0 || choice > nchoice) {
        return ESP_ERR_INVALID_STATE;
    }
    *out_addr = choices[choice - 1].addr;
    return ESP_OK;
}
#endif /* BLE */

static const char *prop_type_str(uint32_t type)
{
    switch (type) {
    case PROP_TYPE_TIMESTAMP:
        return "TIME(us)";
    case PROP_TYPE_INT32:
        return "INT32";
    case PROP_TYPE_BOOLEAN:
        return "BOOL";
    case PROP_TYPE_STRING:
        return "STRING";
    default:
        return "CUSTOM";
    }
}

static void print_prop_value(const esp_local_controller_prop_t *p)
{
    if (!p->value || p->value_len == 0) {
        printf("(empty)");
        return;
    }
    switch (p->type) {
    case PROP_TYPE_TIMESTAMP:
        if (p->value_len >= sizeof(int64_t)) {
            int64_t ts = 0;
            memcpy(&ts, p->value, sizeof(ts));
            printf("%" PRId64, ts);
        }
        break;
    case PROP_TYPE_INT32:
        if (p->value_len >= sizeof(int32_t)) {
            int32_t v = 0;
            memcpy(&v, p->value, sizeof(v));
            printf("%" PRId32, v);
        }
        break;
    case PROP_TYPE_BOOLEAN:
        printf("%s", p->value[0] ? "true" : "false");
        break;
    case PROP_TYPE_STRING:
        printf("%.*s", (int)p->value_len, (const char *)p->value);
        break;
    default:
        printf("<%zu bytes>", p->value_len);
        break;
    }
}

static esp_err_t interactive_control(esp_local_controller_t *ctrl)
{
    uint32_t count = 0;
    ESP_RETURN_ON_ERROR(esp_local_controller_get_property_count(ctrl, &count), TAG, "count");
    if (count == 0) {
        printf("No properties on device.\n");
        return ESP_OK;
    }

    uint32_t *indices = calloc(count, sizeof(*indices));
    ESP_RETURN_ON_FALSE(indices, ESP_ERR_NO_MEM, TAG, "indices");
    for (uint32_t i = 0; i < count; i++) {
        indices[i] = i;
    }

    esp_local_controller_prop_t *props = NULL;
    size_t nprops = 0;
    esp_err_t err = esp_local_controller_get_property_values(ctrl, indices, count, &props, &nprops);
    free(indices);
    if (err != ESP_OK) {
        return err;
    }

    printf("\nProperties (%zu):\n", nprops);
    for (size_t i = 0; i < nprops; i++) {
        printf("  [%zu] %s  %s  %s  ", i + 1,
               props[i].name ? props[i].name : "?",
               prop_type_str(props[i].type),
               (props[i].flags & PROP_FLAG_READONLY) ? "Read-Only" : "RW");
        print_prop_value(&props[i]);
        printf("\n");
    }

    int select = read_console_int("Select property to set (0=skip): ");
    if (select > 0 && (size_t)select <= nprops) {
        size_t idx = (size_t)select - 1;
        if (props[idx].flags & PROP_FLAG_READONLY) {
            printf("Property is read-only.\n");
        } else {
            char line[128];
            err = read_console_line_default("Enter new value: ", line, sizeof(line),
                                            NULL, false, true);
            if (err != ESP_OK) {
                goto cleanup;
            }

            esp_local_controller_prop_set_t set = {
                .index = (uint32_t)idx,
            };
            int32_t i32 = 0;
            bool bval = false;
            uint8_t raw[8];

            switch (props[idx].type) {
            case PROP_TYPE_INT32:
                i32 = (int32_t)atoi(line);
                memcpy(raw, &i32, sizeof(i32));
                set.value = raw;
                set.value_len = sizeof(i32);
                break;
            case PROP_TYPE_BOOLEAN:
                bval = (atoi(line) != 0) || (strcasecmp(line, "true") == 0);
                raw[0] = bval ? 1 : 0;
                set.value = raw;
                set.value_len = 1;
                break;
            case PROP_TYPE_STRING:
                set.value = (const uint8_t *)line;
                set.value_len = strlen(line);
                break;
            default:
                printf("Unsupported type for set in this example.\n");
                err = ESP_ERR_NOT_SUPPORTED;
                goto cleanup;
            }

            err = esp_local_controller_set_property_values(ctrl, &set, 1);
            if (err == ESP_OK) {
                printf("Set OK.\n");
            } else {
                ESP_LOGE(TAG, "set failed: %s", esp_err_to_name(err));
            }
        }
    }

cleanup:
    esp_local_controller_props_free(props, nprops);
    return err;
}

static esp_err_t run_session(protocomm_ext_config_data_t *pe_cfg, const void *session_cfg)
{
    pe_cfg->security_method = PROTOCOMM_EXT_SECURITY_METHOD_NONE;
    pe_cfg->security_data = NULL;

    protocomm_ext_t *pc = protocomm_ext_new(pe_cfg);
    ESP_RETURN_ON_FALSE(pc, ESP_ERR_NO_MEM, TAG, "protocomm_ext_new");

    esp_local_controller_t *ctrl = esp_local_controller_create(pc);
    if (!ctrl) {
        protocomm_ext_delete(pc);
        return ESP_ERR_NO_MEM;
    }

    esp_err_t err = esp_local_controller_fetch_version(ctrl, session_cfg);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "fetch_version: %s", esp_err_to_name(err));
        goto done;
    }

    esp_local_controller_version_t ver = {0};
    err = esp_local_controller_get_version(ctrl, &ver);
    if (err != ESP_OK) {
        goto done;
    }
    printf("Device local_ctrl ver=%s\n", ver.ver ? ver.ver : "?");

    err = configure_security_from_version(pc, &ver);
    esp_local_controller_version_free(&ver);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "set_security: %s", esp_err_to_name(err));
        goto done;
    }

    err = esp_local_controller_establish_security(ctrl);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "establish_security: %s", esp_err_to_name(err));
        goto done;
    }

    err = interactive_control(ctrl);

done:
    esp_local_controller_stop_session(ctrl);
    esp_local_controller_delete(ctrl);
    protocomm_ext_delete(pc);
    clear_security_credentials();
    return err;
}

static int pick_mode(void)
{
    printf("\nESP Local Controller — select transport:\n");
#if CONFIG_EXAMPLE_SUPPORT_HTTP
    printf("  1) HTTP\n");
#endif
#if CONFIG_EXAMPLE_SUPPORT_BLE
    printf("  2) BLE\n");
#endif
#if CONFIG_EXAMPLE_SUPPORT_CONSOLE
    printf("  3) Console (UART)\n");
#endif
    return read_console_int("Select: ");
}

void app_main(void)
{
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

#if CONFIG_EXAMPLE_SUPPORT_HTTP
    static bool wifi_ready;
#endif

    while (1) {
        int mode = pick_mode();
        protocomm_ext_config_data_t pe_cfg = {0};
        const void *session_cfg = NULL;
#if CONFIG_EXAMPLE_SUPPORT_BLE
        static ble_addr_t peer_addr;
#endif
#if CONFIG_EXAMPLE_SUPPORT_HTTP
        static esp_http_client_config_t http_cfg;
        static char http_url[128];
#endif
#if CONFIG_EXAMPLE_SUPPORT_CONSOLE
        static protocomm_ext_console_config_t console_cfg;
#endif

        switch (mode) {
#if CONFIG_EXAMPLE_SUPPORT_HTTP
        case MODE_HTTP: {
            if (!wifi_ready) {
                if (wifi_sta_connect() != ESP_OK) {
                    continue;
                }
                wifi_ready = true;
            }
            strncpy(http_url, CONFIG_EXAMPLE_HTTP_URL, sizeof(http_url) - 1);
            char prompt[160];
            snprintf(prompt, sizeof(prompt), "HTTP URL [default: %s]: ", CONFIG_EXAMPLE_HTTP_URL);
            if (read_console_line_default(prompt, http_url, sizeof(http_url),
                                          CONFIG_EXAMPLE_HTTP_URL, false, true) != ESP_OK) {
                continue;
            }
            memset(&http_cfg, 0, sizeof(http_cfg));
            http_cfg.url = http_url;
            http_cfg.timeout_ms = 10000;
            pe_cfg.transport_method = PROTOCOMM_EXT_TRANSPORT_METHOD_HTTP;
            pe_cfg.transport_data = &http_cfg;
            break;
        }
#endif
#if CONFIG_EXAMPLE_SUPPORT_BLE
        case MODE_BLE:
            pe_cfg.transport_method = PROTOCOMM_EXT_TRANSPORT_METHOD_BLE;
            pe_cfg.transport_data = NULL;
            /* Need instance first to scan — create temp, pick, then recreate below */
            {
                protocomm_ext_config_data_t scan_cfg = pe_cfg;
                scan_cfg.security_method = PROTOCOMM_EXT_SECURITY_METHOD_NONE;
                protocomm_ext_t *scan_pc = protocomm_ext_new(&scan_cfg);
                if (!scan_pc) {
                    ESP_LOGE(TAG, "scan protocomm_ext_new failed");
                    continue;
                }
                if (ble_pick_device(scan_pc, &peer_addr) != ESP_OK) {
                    protocomm_ext_delete(scan_pc);
                    continue;
                }
                protocomm_ext_delete(scan_pc);
                session_cfg = &peer_addr;
            }
            break;
#endif
#if CONFIG_EXAMPLE_SUPPORT_CONSOLE
        case MODE_CONSOLE:
            memset(&console_cfg, 0, sizeof(console_cfg));
            console_cfg.uart_num = UART_NUM_0;
            console_cfg.baud_rate = 115200;
            console_cfg.timeout_ms = 5000;
            pe_cfg.transport_method = PROTOCOMM_EXT_TRANSPORT_METHOD_CONSOLE;
            pe_cfg.transport_data = &console_cfg;
            break;
#endif
        default:
            printf("Invalid selection.\n");
            continue;
        }

        esp_err_t err = run_session(&pe_cfg, session_cfg);
        printf("Session finished: %s\n", esp_err_to_name(err));
    }
}
