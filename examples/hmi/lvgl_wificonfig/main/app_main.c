// Copyright 2015-2018 Espressif Systems (Shanghai) PTE LTD
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at

//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "freertos/event_groups.h"
#include "esp_freertos_hooks.h"
#include "esp_log.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "nvs_flash.h"
#include "board.h"
#include "lvgl_gui.h"


#define TAG "lvgl_wificonfig"
#define WIFI_CONNECT_TIMEOUT 20000
#define WIFI_SCAN_TIMEOUT 10000

/**********************
 *      TYPEDEFS
 **********************/
/** @brief Description of an WiFi AP */
typedef struct {
    uint8_t bssid[6];                     /**< MAC address of AP */
    char ssid[33];                        /**< SSID of AP */
    uint8_t primary;                      /**< channel of AP */
    wifi_second_chan_t second;            /**< secondary channel of AP */
    int8_t  rssi;                         /**< signal strength of AP */
    wifi_auth_mode_t authmode;            /**< authmode of AP */
} ap_info_t;

typedef struct {
    uint16_t apCount;
    ap_info_t *ap_info_list;
    uint16_t current_ap;
    const char *current_pwd;
} wifi_config_data_t;

static wifi_config_data_t wifi_config_data;

/**********************
 *  STATIC VARIABLES
 **********************/
LV_FONT_DECLARE(password_eye_20);


static lv_obj_t *g_preload           = NULL;
static lv_obj_t *g_wifi_config_label = NULL;
static lv_obj_t *g_contain           = NULL;
static lv_obj_t *g_wifi_list         = NULL;
static lv_obj_t *kb                  = NULL;
static lv_obj_t *btn_connect         = NULL;
static lv_obj_t *pwd_ta              = NULL;
static lv_obj_t *g_pswd_page         = NULL;
static lv_obj_t *g_ap_page           = NULL;

/* FreeRTOS event group to signal when we are connected & ready to make a request */
static EventGroupHandle_t g_wifi_event_group  = NULL;

/** The event group allows multiple bits for each event,
 * but we only care about one event - are we connected to the AP with an IP? */
static const uint32_t LVGL_WIFI_CONFIG_CONNECTED       = BIT0;
static const uint32_t LVGL_WIFI_CONFIG_SCAN            = BIT1;
static const uint32_t LVGL_WIFI_CONFIG_SCAN_DONE       = BIT2;
static const uint32_t LVGL_WIFI_CONFIG_CONNECT_FAIL    = BIT4;
static const uint32_t LVGL_WIFI_CONFIG_TRY_CONNECT     = BIT5;


static esp_timer_handle_t g_wifi_timeout_timer  = NULL;


static char *wifi_auth_mode_to_str(wifi_auth_mode_t mode)
{
    switch (mode) {
    case WIFI_AUTH_OPEN : return "OPEN";
    case WIFI_AUTH_WEP: return "WEP";
    case WIFI_AUTH_WPA_PSK: return "WPA_PSK";
    case WIFI_AUTH_WPA2_PSK: return "WPA2_PSK";
    case WIFI_AUTH_WPA_WPA2_PSK: return "WPA_WPA2_PSK";
    case WIFI_AUTH_WPA2_ENTERPRISE: return "WPA2_ENTERPRISE";
    case WIFI_AUTH_WPA3_PSK: return "WPA3_PSK";
    case WIFI_AUTH_WPA2_WPA3_PSK: return "WPA2_WPA3_PSK";
    default:
        break;
    }
    return "auth mode error";
}

static void wifi_timeout_timer_create(esp_timer_cb_t callback, const char *name, uint32_t time_ms)
{
    esp_timer_create_args_t timer_conf = {
        .callback = callback,
        .name = name
    };
    if (NULL != g_wifi_timeout_timer) {
        ESP_LOGE(TAG, "Creat failed, timeout timer has been created");
        return;
    }
    ESP_LOGI(TAG, "start timer: %s", name);
    esp_timer_create(&timer_conf, &g_wifi_timeout_timer);
    esp_timer_start_once(g_wifi_timeout_timer, time_ms * 1000U);
}

static void wifi_timeout_timer_delete(void)
{
    if (g_wifi_timeout_timer) {
        esp_timer_stop(g_wifi_timeout_timer);
        esp_timer_delete(g_wifi_timeout_timer);
        g_wifi_timeout_timer = NULL;
    }
}


static void preload_create(void)
{
    /* wifi scan preload create */
    g_preload = lv_spinner_create(g_contain, NULL);
    lv_obj_set_size(g_preload, 100, 100);
    lv_obj_align(g_preload, NULL, LV_ALIGN_CENTER, 0, 0);
}

static void preload_delete(void)
{
    if (NULL != g_preload) {
        lv_obj_del(g_preload);
        g_preload = NULL;
    }
}


/**!<  ----------- */

static void wifi_connect_fail(void)
{
    preload_delete();
    ESP_LOGW(TAG, "connect failed");
}

static void wifi_connect_success(void)
{
    preload_delete();
    lv_obj_del(g_pswd_page);
    g_pswd_page = NULL;

    lv_obj_t *btn = lv_list_get_btn_selected(g_wifi_list);
    lv_btn_set_checkable(btn, false);
    static lv_style_t style_btn;
    lv_style_init(&style_btn);
    lv_style_set_bg_color(&style_btn, LV_STATE_DEFAULT, LV_COLOR_BLUE);
    lv_obj_add_style(btn, LV_BTN_PART_MAIN, &style_btn);

}

static void pswd_page_click_connect_cb(lv_obj_t *obj, lv_event_t event)
{
    if (LV_EVENT_CLICKED == event) {
        const char *pwd = lv_textarea_get_text(pwd_ta);
        printf("pwd=%s\n", pwd);
        lv_obj_t *wifi_btn = lv_list_get_btn_selected(g_wifi_list);
        int32_t index = lv_list_get_btn_index(g_wifi_list, wifi_btn);
        wifi_config_data.current_ap = index;
        wifi_config_data.current_pwd = pwd;
        preload_create();
        xEventGroupSetBits(g_wifi_event_group, LVGL_WIFI_CONFIG_TRY_CONNECT);
    }
}

static void pswd_page_click_cancel_cb(lv_obj_t *obj, lv_event_t event)
{
    if (LV_EVENT_CLICKED == event) {

        lv_obj_del(g_pswd_page);
        g_pswd_page = NULL;
    }
}

static void ta_event_cb(lv_obj_t *ta, lv_event_t event)
{
    if (event == LV_EVENT_CLICKED) {
        /* Focus on the clicked text area */
        if (kb != NULL) {
            lv_keyboard_set_textarea(kb, ta);
        }
    }

    else if (event == LV_EVENT_INSERT) {
        const char *str = lv_event_get_data();
        if (str[0] == '\n') {
            ESP_LOGI(TAG, "Ready");
        }
    }
}

static void kb_event_cb(lv_obj_t *kb, lv_event_t event)
{
    lv_keyboard_def_event_cb(kb, event);

    if (LV_EVENT_VALUE_CHANGED == event) {
        const char *pwd = lv_textarea_get_text(pwd_ta);
        if (strlen(pwd) >= 8) {
            lv_btn_set_state(btn_connect, LV_BTN_STATE_RELEASED);
        } else {
            lv_btn_set_state(btn_connect, LV_BTN_STATE_DISABLED);
        }
    }

    if (LV_EVENT_APPLY == event || LV_EVENT_CANCEL == event) {
        // lv_obj_del(kb);
        // kb = NULL;
        if (LV_EVENT_APPLY == event && LV_BTN_STATE_DISABLED != lv_btn_get_state(btn_connect)) {
            /* send clicked event to simulate button clicked */
            pswd_page_click_connect_cb(NULL, LV_EVENT_CLICKED);
        }
    }
}

static void ap_page_click_connect_cb(lv_obj_t *obj, lv_event_t event)
{
    if (LV_EVENT_CLICKED == event) {

        g_pswd_page = lv_cont_create(g_contain, NULL);
        lv_obj_set_size(g_pswd_page, lv_obj_get_width(g_contain), lv_obj_get_height(g_contain));

        /* Create a label and position it above the text box */
        lv_obj_t *wifi_btn = lv_list_get_btn_selected(g_wifi_list);
        int32_t index = lv_list_get_btn_index(g_wifi_list, wifi_btn);

        lv_obj_t *pwd_label = lv_label_create(g_pswd_page, NULL);
        lv_label_set_text_fmt(pwd_label, "%s Password:", wifi_config_data.ap_info_list[index].ssid);
        lv_obj_align(pwd_label, NULL, LV_ALIGN_IN_TOP_MID, 0, 0);


        /* Create the password box */
        pwd_ta = lv_textarea_create(g_pswd_page, NULL);
        lv_obj_align(pwd_ta, NULL, LV_ALIGN_IN_TOP_MID, 0, 22);
        lv_textarea_set_text(pwd_ta, "");
        lv_textarea_set_pwd_mode(pwd_ta, true);
        lv_textarea_set_one_line(pwd_ta, true);
        lv_textarea_set_cursor_hidden(pwd_ta, true);
        lv_obj_set_event_cb(pwd_ta, ta_event_cb);

        lv_obj_t *label;
        lv_obj_t *btn_cancel = lv_btn_create(g_pswd_page, NULL);
        lv_obj_set_size(btn_cancel, (80), (30));
        lv_obj_set_event_cb(btn_cancel, pswd_page_click_cancel_cb);
        lv_obj_align(btn_cancel, pwd_ta, LV_ALIGN_OUT_BOTTOM_MID, -80, 5);
        label = lv_label_create(btn_cancel, NULL);
        lv_label_set_text(label, "Canael");

        btn_connect = lv_btn_create(g_pswd_page, NULL);
        lv_obj_set_size(btn_connect, (80), (30));
        lv_obj_set_event_cb(btn_connect, pswd_page_click_connect_cb);
        lv_obj_align(btn_connect, pwd_ta, LV_ALIGN_OUT_BOTTOM_MID, 80, 5);
        lv_btn_set_state(btn_connect, LV_BTN_STATE_DISABLED);
        label = lv_label_create(btn_connect, NULL);
        lv_label_set_text(label, "Connect");

        /* Create a keyboard */
        kb = lv_keyboard_create(g_pswd_page, NULL);
        lv_obj_set_size(kb,  lv_obj_get_width(g_pswd_page), lv_obj_get_height(g_pswd_page) - 95);
        lv_obj_align(kb, NULL, LV_ALIGN_IN_BOTTOM_MID, 0, 0);
        lv_obj_set_event_cb(kb, kb_event_cb);
        lv_keyboard_set_textarea(kb, pwd_ta); /* Focus it on one of the text areas to start */
        lv_keyboard_set_cursor_manage(kb, true); /* Automatically show/hide cursors on text areas */

        /* delete ap info page */
        lv_obj_del(g_ap_page);
        g_ap_page = NULL;
    }
}

static void ap_page_click_cancel_cb(lv_obj_t *obj, lv_event_t event)
{
    if (LV_EVENT_CLICKED == event) {

        lv_obj_del(g_ap_page);
        g_ap_page = NULL;
    }
}

static void click_wifi_list_cb(lv_obj_t *obj, lv_event_t event)
{
    if (LV_EVENT_CLICKED == event) {
        lv_obj_t *wifi_btn = lv_list_get_btn_selected(g_wifi_list);
        int32_t index = lv_list_get_btn_index(g_wifi_list, wifi_btn);

        if (NULL != g_ap_page) {
            lv_obj_del(g_ap_page);
            g_ap_page = NULL;
        }

        g_ap_page = lv_page_create(g_contain, NULL);
        lv_obj_set_size(g_ap_page, lv_obj_get_width(g_contain) - 20, lv_obj_get_height(g_contain) / 2);
        lv_obj_align(g_ap_page, NULL, LV_ALIGN_IN_BOTTOM_MID, 0, -5);

        lv_obj_t *ssid_label = lv_label_create(g_ap_page, NULL);
        lv_obj_set_width(ssid_label, lv_obj_get_width(g_ap_page));
        lv_obj_align(ssid_label, NULL, LV_ALIGN_IN_TOP_LEFT, 0, 0);
        char info_str[128] = {0};
        sprintf(info_str, "SSID: %s\nRSSI: %d\nAuth: %s",
                wifi_config_data.ap_info_list[index].ssid,
                wifi_config_data.ap_info_list[index].rssi,
                wifi_auth_mode_to_str(wifi_config_data.ap_info_list[index].authmode)
               );
        lv_label_set_text(ssid_label, info_str);
        lv_label_set_align(ssid_label, LV_LABEL_ALIGN_LEFT);

        lv_obj_t *label;
        lv_obj_t *btn_cancel = lv_btn_create(g_ap_page, NULL);
        lv_obj_set_size(btn_cancel, (80), (30));
        lv_obj_set_event_cb(btn_cancel, ap_page_click_cancel_cb);
        lv_obj_align(btn_cancel, NULL, LV_ALIGN_IN_BOTTOM_LEFT, 0, 0);
        label = lv_label_create(btn_cancel, NULL);
        lv_label_set_text(label, "Canael");

        lv_obj_t *btn_connect = lv_btn_create(g_ap_page, NULL);
        lv_obj_set_size(btn_connect, (80), (30));
        lv_obj_set_event_cb(btn_connect, ap_page_click_connect_cb);
        lv_obj_align(btn_connect, NULL, LV_ALIGN_IN_BOTTOM_RIGHT, 0, 0);
        label = lv_label_create(btn_connect, NULL);
        lv_label_set_text(label, "Connect");
    }
}

static void wifi_scan_timeout_cb(void *timer)
{
    preload_delete();
    lv_label_set_text(g_wifi_config_label, "Wi-Fi Config");
    ESP_LOGW(TAG, "scan timeout");
}

static void wifi_scan_done(void)
{
    wifi_timeout_timer_delete();
    preload_delete();
    lv_label_set_text(g_wifi_config_label, "Wi-Fi Config");
    wifi_config_data_t *data = &wifi_config_data;

    lv_obj_t *list_btn;
    for (uint8_t i = 0; i < data->apCount; i++) {
        list_btn = lv_list_add_btn(g_wifi_list, LV_SYMBOL_WIFI, (char *)data->ap_info_list[i].ssid);
        lv_obj_set_event_cb(list_btn, click_wifi_list_cb);
    }
}

static void start_wifi_scan(void)
{
    lv_label_set_text(g_wifi_config_label, "Scaning...");
    preload_create();
    wifi_timeout_timer_create(wifi_scan_timeout_cb, "scan timer", WIFI_SCAN_TIMEOUT);
}

static void wifi_scan_start(void)
{
    ESP_LOGI(TAG, "[ * ] wifi scan");
    xEventGroupSetBits(g_wifi_event_group, LVGL_WIFI_CONFIG_SCAN);
}

static void littlevgl_wificonfig_init(void)
{
    ESP_LOGI(TAG, "[ * ] littlevgl wificonfig");

    /* wifi config container create */
    g_contain = (lv_scr_act());

    /* wifi config label create */
    g_wifi_config_label = lv_label_create(g_contain, NULL);
    lv_label_set_text(g_wifi_config_label, "Wi-Fi Config");
    lv_obj_align(g_wifi_config_label, g_contain, LV_ALIGN_IN_TOP_MID, 0, 0);

    g_wifi_list = lv_list_create(g_contain, NULL);
    lv_obj_set_size(g_wifi_list, lv_obj_get_width(g_contain), lv_obj_get_height(g_contain) - lv_obj_get_height(g_wifi_config_label));
    lv_obj_align(g_wifi_list, NULL, LV_ALIGN_IN_TOP_LEFT, 0, lv_obj_get_height(g_wifi_config_label));

}

static void net_event_handler(void *arg, esp_event_base_t event_base,
                              int32_t event_id, void *event_data)
{
    static int s_disconnected_handshake_count = 0;
    static int s_disconnected_unknown_count = 0;

    if (event_base == WIFI_EVENT) {
        switch (event_id) {
        case WIFI_EVENT_STA_START:

            break;
        case WIFI_EVENT_STA_CONNECTED:
            s_disconnected_unknown_count = 0;
            s_disconnected_handshake_count = 0;
            wifi_event_sta_connected_t *sta_connected = event_data;
            ESP_LOGI(TAG, "connected ssid: %s", sta_connected->ssid);
            break;
        case WIFI_EVENT_STA_DISCONNECTED: {
            uint8_t sta_conn_state = 0;
            wifi_event_sta_disconnected_t *disconnected = event_data;
            switch (disconnected->reason) {
            case WIFI_REASON_ASSOC_TOOMANY:
                ESP_LOGW(TAG, "WIFI_REASON_ASSOC_TOOMANY Disassociated because AP is unable to handle all currently associated STAs");
                ESP_LOGW(TAG, "The number of connected devices on the router exceeds the limit");

                sta_conn_state = 1;

                break;

            case WIFI_REASON_MIC_FAILURE:              /**< disconnected reason code 14 */
            case WIFI_REASON_4WAY_HANDSHAKE_TIMEOUT:   /**< disconnected reason code 15 */
            case WIFI_REASON_GROUP_KEY_UPDATE_TIMEOUT: /**< disconnected reason code 16 */
            case WIFI_REASON_IE_IN_4WAY_DIFFERS:       /**< disconnected reason code 17 */
            case WIFI_REASON_HANDSHAKE_TIMEOUT:        /**< disconnected reason code 204 */
                ESP_LOGW(TAG, "Wi-Fi 4-way handshake failed, count: %d", s_disconnected_handshake_count);

                if (++s_disconnected_handshake_count >= 3) {
                    ESP_LOGW(TAG, "Router password error");
                    sta_conn_state = 2;
                }
                break;

            default:
                if (++s_disconnected_unknown_count > 10) {
                    ESP_LOGW(TAG, "Router password error");
                    sta_conn_state = 3;
                }
                break;
            }

            if (sta_conn_state == 0) {
                ESP_ERROR_CHECK(esp_wifi_connect());
            }
            xEventGroupSetBits(g_wifi_event_group, LVGL_WIFI_CONFIG_CONNECT_FAIL);
            break;
        }
        case WIFI_EVENT_SCAN_DONE: {
            wifi_event_sta_scan_done_t *scan_done_data = event_data;
            uint16_t apCount = scan_done_data->number;
            if (apCount == 0) {
                ESP_LOGI(TAG, "[ * ] Nothing AP found");
                break;
            }

            wifi_ap_record_t *wifi_ap_list = (wifi_ap_record_t *)malloc(sizeof(wifi_ap_record_t) * apCount);
            if (!wifi_ap_list) {
                ESP_LOGE(TAG, "[ * ] malloc error, wifi_ap_list is NULL");
                break;
            }
            ESP_ERROR_CHECK(esp_wifi_scan_get_ap_records(&apCount, wifi_ap_list));

            wifi_config_data.apCount = apCount;
            wifi_config_data.ap_info_list = (ap_info_t *)malloc(apCount * sizeof(ap_info_t));
            if (!wifi_config_data.ap_info_list) {
                ESP_LOGE(TAG, "[ * ] realloc error, ap_info_list is NULL");
                break;
            }
            for (int i = 0; i < apCount; ++i) {
                wifi_config_data.ap_info_list[i].rssi = wifi_ap_list[i].rssi;
                wifi_config_data.ap_info_list[i].authmode = wifi_ap_list[i].authmode;
                memcpy(wifi_config_data.ap_info_list[i].ssid, wifi_ap_list[i].ssid, sizeof(wifi_ap_list[i].ssid));
            }
            xEventGroupSetBits(g_wifi_event_group, LVGL_WIFI_CONFIG_SCAN_DONE);
            esp_wifi_scan_stop();
            free(wifi_ap_list);
            break;
        }
        default:
            break;
        }
    } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        xEventGroupSetBits(g_wifi_event_group, LVGL_WIFI_CONFIG_CONNECTED);
    }
}

static void initialise_wifi(void)
{
    g_wifi_event_group = xEventGroupCreate();

    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_create_default_wifi_sta();

    esp_event_handler_instance_t instance_any_id;
    esp_event_handler_instance_t instance_got_ip;
    ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT,
                    ESP_EVENT_ANY_ID,
                    &net_event_handler,
                    NULL,
                    &instance_any_id));
    ESP_ERROR_CHECK(esp_event_handler_instance_register(IP_EVENT,
                    IP_EVENT_STA_GOT_IP,
                    &net_event_handler,
                    NULL,
                    &instance_got_ip));

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));
    ESP_ERROR_CHECK(esp_wifi_set_storage(WIFI_STORAGE_RAM));
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_start());
}

static void wifi_config_event_task(void *args)
{
    while (1) {
        EventBits_t uxBits;
        uxBits = xEventGroupWaitBits(g_wifi_event_group,
                                     (LVGL_WIFI_CONFIG_SCAN | LVGL_WIFI_CONFIG_SCAN_DONE | LVGL_WIFI_CONFIG_CONNECTED | LVGL_WIFI_CONFIG_CONNECT_FAIL | LVGL_WIFI_CONFIG_TRY_CONNECT),
                                     pdTRUE, pdFALSE, portMAX_DELAY);

        switch (uxBits) {
        case LVGL_WIFI_CONFIG_SCAN: {
            wifi_scan_config_t scanConf = {
                .ssid = NULL,
                .bssid = NULL,
                .channel = 0,
                .show_hidden = false
            };
            ESP_ERROR_CHECK(esp_wifi_scan_start(&scanConf, false));
            start_wifi_scan();
            break;
        }
        case LVGL_WIFI_CONFIG_SCAN_DONE: {
            ESP_LOGI(TAG, "[ * ] running refresh wifi list：%d\n", wifi_config_data.apCount);
            wifi_scan_done();
            break;
        }
        case LVGL_WIFI_CONFIG_CONNECTED:
            wifi_connect_success();
            break;

        case LVGL_WIFI_CONFIG_CONNECT_FAIL:
            esp_wifi_disconnect();
            wifi_connect_fail();
            break;

        case LVGL_WIFI_CONFIG_TRY_CONNECT: {
            wifi_config_t sta_config = {0};
            strcpy((char *)sta_config.sta.ssid, wifi_config_data.ap_info_list[wifi_config_data.current_ap].ssid);
            strcpy((char *)sta_config.sta.password, wifi_config_data.current_pwd);
            ESP_LOGI(TAG, "[ * ] Select SSID：%s", sta_config.sta.ssid);
            ESP_LOGI(TAG, "[ * ] Input Password：%s", sta_config.sta.password);
            esp_wifi_set_config(ESP_IF_WIFI_STA, &sta_config);
            esp_wifi_disconnect();
            esp_wifi_connect();
            break;
        }

        default:
            break;
        }
    }
}

/******************************************************************************
 * FunctionName : app_main
 * Description  : entry of user application, init user function here
 * Parameters   : none
 * Returns      : none
*******************************************************************************/
void app_main()
{
    /* Initialize NVS */
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    iot_board_init();
    spi_bus_handle_t spi2_bus = iot_board_get_handle(BOARD_SPI2_ID);

    scr_driver_t lcd_drv;
    touch_panel_driver_t touch_drv;
    scr_interface_spi_config_t spi_lcd_cfg = {
        .spi_bus = spi2_bus,
        .pin_num_cs = BOARD_LCD_SPI_CS_PIN,
        .pin_num_dc = BOARD_LCD_SPI_DC_PIN,
        .clk_freq = BOARD_LCD_SPI_CLOCK_FREQ,
        .swap_data = true,
    };

    scr_interface_driver_t *iface_drv;
    scr_interface_create(SCREEN_IFACE_SPI, &spi_lcd_cfg, &iface_drv);

    scr_controller_config_t lcd_cfg = {
        .interface_drv = iface_drv,
        .pin_num_rst = 18,
        .pin_num_bckl = 23,
        .rst_active_level = 0,
        .bckl_active_level = 1,
        .offset_hor = 0,
        .offset_ver = 0,
        .width = 240,
        .height = 320,
        .rotate = SCR_DIR_TBLR,
    };
    scr_find_driver(SCREEN_CONTROLLER_ILI9341, &lcd_drv);
    lcd_drv.init(&lcd_cfg);

    touch_panel_config_t touch_cfg = {
        .interface_spi = {
            .spi_bus = spi2_bus,
            .pin_num_cs = BOARD_TOUCH_SPI_CS_PIN,
            .clk_freq = 10000000,
        },
        .interface_type = TOUCH_PANEL_IFACE_SPI,
        .pin_num_int = -1,
        .direction = TOUCH_DIR_TBLR,
        .width = 240,
        .height = 320,
    };
    touch_panel_find_driver(TOUCH_PANEL_CONTROLLER_XPT2046, &touch_drv);
    touch_drv.init(&touch_cfg);
    touch_drv.calibration_run(&lcd_drv, false);
    /* Initialize LittlevGL GUI */
    lvgl_init(&lcd_drv, &touch_drv);

    littlevgl_wificonfig_init();

    /* initialise wifi */
    initialise_wifi();
    xTaskCreate(wifi_config_event_task, "config_task", 2048, NULL, 4, NULL);

    vTaskDelay(pdMS_TO_TICKS(1000));
    wifi_scan_start();

    ESP_LOGI(TAG, "[APP] IDF version: %s", esp_get_idf_version());
    ESP_LOGI(TAG, "[APP] Free memory: %d bytes", esp_get_free_heap_size());
}
