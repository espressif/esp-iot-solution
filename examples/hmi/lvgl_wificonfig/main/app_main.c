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

/* component includes */
#include <stdio.h>

/* freertos includes */
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/timers.h"
#include "freertos/semphr.h"
#include "freertos/queue.h"
#include "freertos/event_groups.h"
#include "esp_freertos_hooks.h"

/* esp includes */
#include "sdkconfig.h"
#include "esp_log.h"
#include "esp_err.h"
#include "esp_wifi.h"
#include "esp_event_loop.h"

/* Param Include */
#include "iot_param.h"
#include "nvs_flash.h"

/* lvgl includes */
#include "iot_lvgl.h"

/*********************
 *      DEFINES
 *********************/
#define SYMBOL_EYE_ON "\xEF\x81\xAE"
#define SYMBOL_EYE_OFF "\xEF\x81\xB0"

#define TAG "lvgl_wificonfig"
#define WIFI_CONNECT_TIMEOUT 10000
#define WIFI_SCAN_TIMEOUT 10000

/**********************
 *      TYPEDEFS
 **********************/
/** @brief Description of an WiFi AP */
typedef struct {
    uint8_t ssid[33]; /**< SSID of AP */
    int8_t rssi;      /**< signal strength of AP */
} ap_list_t;

typedef enum {
    LVGL_WIFI_CONFIG_SCAN = 0,
    LVGL_WIFI_CONFIG_SCAN_DONE,
    LVGL_WIFI_CONFIG_CONNECT_SUCCESS,
    LVGL_WIFI_CONFIG_CONNECT_FAIL,
    LVGL_WIFI_CONFIG_TRY_CONNECT,
} wifi_config_event_t;

typedef struct {
    wifi_config_event_t event;
    void *ctx;
} wifi_config_data_t;

/**********************
 *  STATIC VARIABLES
 **********************/
LV_FONT_DECLARE(password_eye_20);

static lv_obj_t *connectedap        = NULL;
static lv_obj_t *connectedaprssi    = NULL;
static lv_obj_t *connectedapbssid   = NULL;
static lv_obj_t *connectedapchannel = NULL;

static lv_obj_t *wifi_connect_cont  = NULL;
static lv_obj_t *connect_label      = NULL;
static lv_obj_t *passlabel          = NULL;
static lv_obj_t *passkb             = NULL;
static lv_obj_t *preload            = NULL;
static lv_obj_t *pass_eye           = NULL;
static lv_obj_t *img_eye            = NULL;
static lv_obj_t *pass               = NULL;

static lv_obj_t *wifi_list_cont     = NULL;
static lv_obj_t *wifilist           = NULL;
static lv_obj_t * *list_ap          = NULL;
static lv_obj_t *scan_label         = NULL;

static void *img_src[] = {SYMBOL_EYE_OFF, SYMBOL_EYE_ON};

/* FreeRTOS event group to signal when we are connected & ready to make a request */
static EventGroupHandle_t wifi_event_group  = NULL;
static xQueueHandle g_event_queue_handle    = NULL;

/** The event group allows multiple bits for each event,
 * but we only care about one event - are we connected to the AP with an IP? */
const int CONNECTED_BIT = BIT0;

/* wifi infomation */
static uint16_t apCount         = 0;
static uint16_t current_ap      = 0;
static ap_list_t *ap_list       = NULL;
static char current_appass[64]  = {0};
static char current_apbssid[6]  = {0};
static uint8_t current_apchannel = 0;

static esp_timer_handle_t g_wifi_connect_timer  = NULL;
static esp_timer_handle_t g_wifi_scan_timer     = NULL;

/**********************
 *  STATIC PROTOTYPES
 **********************/
static void wifi_connect_timer_delete(void);
static void wifi_try_connect(void);
static void input_password();

/**********************
 *   STATIC FUNCTIONS
 **********************/
static void lv_object_delete(lv_obj_t *object)
{
    if (object != NULL) {
        lv_obj_del(object);
        object = NULL;
    }
}

/**
 * Called when the button is pressed on the mbox
 * @param btn pointer to the button
 * @param txt pointer to the button txt
 * @return
 */
static lv_res_t mbox_action(lv_obj_t *btn, const char *txt)
{
    ESP_LOGI(TAG, "[ * ] mbox action， btn: %s", txt);
    lv_obj_t *mbox = lv_mbox_get_from_btn(btn);

    if (!strcmp(txt, "Retry")) {
        wifi_try_connect();
    } else if (!strcmp(txt, "ReInput")) {
        input_password();
        lv_ta_set_text(pass, current_appass);
    } else if (!strcmp(txt, "Cancel")) {
        lv_object_delete(connect_label);
        lv_object_delete(wifi_connect_cont);
        lv_object_delete(wifilist);
        lv_object_delete(wifi_list_cont);
    }

    lv_object_delete(mbox);
    return LV_RES_OK;
}

static void wifi_connect_success()
{
    ESP_LOGI(TAG, "[ * ] wifi connect success");

    wifi_connect_timer_delete();

    lv_label_set_text(connectedap, (char *)ap_list[current_ap].ssid);
    char data[64] = {0};
    sprintf(data, "%d", ap_list[current_ap].rssi);
    lv_label_set_text(connectedaprssi, data);
    sprintf(data, "%02x%02x%02x%02x%02x%02x", current_apbssid[0], current_apbssid[1], current_apbssid[2], current_apbssid[3], current_apbssid[4], current_apbssid[5]);
    lv_label_set_text(connectedapbssid, data);
    sprintf(data, "%d", current_apchannel);
    lv_label_set_text(connectedapchannel, data);
    lv_object_delete(preload);
    lv_object_delete(connect_label);
    lv_object_delete(wifi_connect_cont);
    lv_object_delete(wifilist);
    lv_object_delete(wifi_list_cont);
}

static void wifi_connect_fail()
{
    ESP_LOGI(TAG, "[ * ] wifi connect fail");
    /*Add styles*/
    static lv_style_t bg;
    static lv_style_t btn_bg;
    lv_style_copy(&bg, &lv_style_pretty);
    lv_style_copy(&btn_bg, &lv_style_pretty);
    bg.body.padding.hor = 20;
    bg.body.padding.ver = 20;
    bg.body.padding.inner = 20;
    bg.body.main_color = LV_COLOR_BLACK;
    bg.body.grad_color = LV_COLOR_MAROON;
    bg.text.color = LV_COLOR_WHITE;

    btn_bg.body.padding.hor = 10;
    btn_bg.body.padding.ver = 5;
    btn_bg.body.padding.inner = 40;
    btn_bg.body.empty = 1;
    btn_bg.body.border.color = LV_COLOR_WHITE;
    btn_bg.text.color = LV_COLOR_WHITE;

    static lv_style_t btn_rel;
    lv_style_copy(&btn_rel, &lv_style_btn_rel);
    btn_rel.body.empty = 1;
    btn_rel.body.border.color = LV_COLOR_WHITE;

    /*Add buttons and modify text*/
    static const char *mboxbtns[] = {"ReInput", "Cancel", ""};
    lv_obj_t *mbox = lv_mbox_create(lv_scr_act(), NULL);
    lv_mbox_add_btns(mbox, mboxbtns, NULL);
    lv_mbox_set_text(mbox, "Wi-Fi Connect Fail!");
    lv_obj_set_width(mbox, LV_HOR_RES / 4 * 3);
    lv_mbox_set_style(mbox, LV_MBOX_STYLE_BTN_REL, &btn_rel);
    lv_mbox_set_style(mbox, LV_MBOX_STYLE_BTN_BG, &btn_bg);
    lv_mbox_set_style(mbox, LV_MBOX_STYLE_BG, &bg);
    lv_obj_align(mbox, NULL, LV_ALIGN_CENTER, 0, -20);
    lv_mbox_set_action(mbox, mbox_action);

    lv_label_set_text(connectedap, "NULL");
    lv_label_set_text(connectedaprssi, "NULL");
    lv_label_set_text(connectedapbssid, "NULL");
    lv_label_set_text(connectedapchannel, "NULL");

    lv_object_delete(preload);
    lv_object_delete(connect_label);
}

static void wifi_connect_timeout_cb(void *timer)
{
    ESP_LOGI(TAG, "[ * ] wifi connect timeout");
    wifi_config_data_t event_data = {0};
    event_data.event = LVGL_WIFI_CONFIG_CONNECT_FAIL;
    xQueueSend(g_event_queue_handle, &event_data, portMAX_DELAY);
}

static void wifi_connect_timer_delete(void)
{
    ESP_LOGI(TAG, "[ * ] wifi connect timer delete");
    if (g_wifi_connect_timer) {
        esp_timer_stop(g_wifi_connect_timer);
        esp_timer_delete(g_wifi_connect_timer);
        g_wifi_connect_timer = NULL;
    }
}

static void wifi_connect_timer_create(void)
{
    ESP_LOGI(TAG, "[ * ] wifi connect timer create");
    esp_timer_create_args_t timer_conf = {
        .callback = wifi_connect_timeout_cb,
        .name = "connect_timeout"
    };

    esp_timer_create(&timer_conf, &g_wifi_connect_timer);

    esp_timer_start_once(g_wifi_connect_timer, WIFI_CONNECT_TIMEOUT * 1000U);
}

static void wifi_try_connect(void)
{
    ESP_LOGI(TAG, "[ * ] wifi try connect");
    connect_label = lv_label_create(wifi_connect_cont, NULL);
    char data[64] = {0};
    sprintf(data, "%s Connecting...", (char *)ap_list[current_ap].ssid);
    lv_label_set_text(connect_label, data);
    lv_obj_align(connect_label, NULL, LV_ALIGN_CENTER, 0, -80);
    
    /* wifi connect preload create */
    preload = lv_preload_create(wifi_connect_cont, NULL);
    lv_obj_set_size(preload, 80, 80);
    lv_obj_align(preload, NULL, LV_ALIGN_CENTER, 0, 0);

    wifi_config_data_t event_data = {0};
    event_data.event = LVGL_WIFI_CONFIG_TRY_CONNECT;
    xQueueSend(g_event_queue_handle, &event_data, portMAX_DELAY);
    wifi_connect_timer_create();
}

/**
 * Called when the ok button is pressed on the keyboard
 * @param keyboard pointer to the keyboard
 * @return
 */
static lv_res_t keyboard_ok_action(lv_obj_t *keyboard)
{
    ESP_LOGI(TAG, "[ * ] keyboard ok action");
    strcpy(current_appass, lv_ta_get_text(pass));

    /* wifi try to connect AP */
    wifi_try_connect();

    lv_object_delete(img_eye);
    lv_object_delete(pass_eye);
    lv_object_delete(passlabel);
    lv_object_delete(pass);
    lv_object_delete(passkb);

    /* LV_RES_INV because the object is deleted */
    return LV_RES_INV;
}

/**
 * Called when the close button is pressed on the keyboard
 * @param keyboard pointer to the keyboard
 * @return
 */
static lv_res_t keyboard_hide_action(lv_obj_t *keyboard)
{
    (void)keyboard; /*Unused*/
    ESP_LOGI(TAG, "[ * ] keyboard hide action");

#if USE_LV_ANIMATION
    lv_obj_animate(passkb, LV_ANIM_FLOAT_BOTTOM | LV_ANIM_OUT, 300, 0, (void (*)(lv_obj_t *))lv_obj_del);
    passkb = NULL;
    return LV_RES_OK;
#else
    lv_object_delete(passkb);
    
    /* LV_RES_INV because the object is deleted */
    return LV_RES_INV;
#endif
}

/**
 * Called when the page of text_area is released
 * @param text_area pointer to the text_area
 * @return
 */
static lv_res_t keyboard_open_close(lv_obj_t *text_area)
{
    (void)text_area; /*Unused*/
    ESP_LOGI(TAG, "[ * ] keyboard open close");

    lv_obj_t *parent = lv_obj_get_parent(lv_obj_get_parent(text_area)); /*Test area is on the scrollable part of the page but we need the page itself*/

    if (passkb) {
        return keyboard_hide_action(passkb);
    } else {

        /* password keyboard create */
        passkb = lv_kb_create(parent, NULL);
        lv_obj_set_height(passkb, LV_VER_RES / 3 * 2);
        lv_obj_align(passkb, NULL, LV_ALIGN_IN_BOTTOM_MID, 0, 0);
        lv_kb_set_ta(passkb, pass);
        lv_kb_set_hide_action(passkb, keyboard_hide_action);
        lv_kb_set_ok_action(passkb, keyboard_ok_action);

#if USE_LV_ANIMATION
        lv_obj_animate(passkb, LV_ANIM_FLOAT_BOTTOM | LV_ANIM_IN, 300, 0, NULL);
#endif
        return LV_RES_OK;
    }
}

/**
 * Called when the button is clicked on password
 * @param obj pointer to the button
 * @return
 */
static lv_res_t pass_eye_action(lv_obj_t *obj)
{
    static bool off = true;
    off = !off;
    if (off) {
        lv_img_set_src(img_eye, img_src[0]);
        lv_ta_set_pwd_mode(pass, true);
    } else {
        lv_img_set_src(img_eye, img_src[1]);
        lv_ta_set_pwd_mode(pass, false);
    }
    return LV_RES_OK;
}

static void input_password()
{
    ESP_LOGI(TAG, "[ * ] keyboard input password");
    passlabel = lv_label_create(wifi_connect_cont, NULL);
    char data[64] = {0};
    sprintf(data, "%s Password:", (char *)ap_list[current_ap].ssid);
    lv_label_set_text(passlabel, data);
    lv_obj_align(passlabel, NULL, LV_ALIGN_IN_TOP_MID, 0, 5);

    /* password text area create */
    pass = lv_ta_create(wifi_connect_cont, NULL);
    lv_obj_set_height(pass, 35);
    lv_ta_set_one_line(pass, true);
    lv_ta_set_pwd_mode(pass, true);
    lv_ta_set_text(pass, "");
    lv_obj_align(pass, NULL, LV_ALIGN_IN_TOP_MID, 0, 30);
    lv_page_set_rel_action(pass, keyboard_open_close);

    /* password eye imgbtn create */
    pass_eye = lv_btn_create(wifi_connect_cont, NULL);
    lv_obj_set_size(pass_eye, 30, 30);
    img_eye = lv_img_create(pass_eye, NULL);
    lv_img_set_src(img_eye, img_src[0]);
    lv_btn_set_action(pass_eye, LV_BTN_ACTION_CLICK, pass_eye_action);
    lv_obj_align(pass_eye, pass, LV_ALIGN_IN_RIGHT_MID, 0, 0);

    /* password keyboard create */
    passkb = lv_kb_create(wifi_connect_cont, NULL);
    lv_obj_set_height(passkb, LV_VER_RES / 3 * 2);
    lv_obj_align(passkb, NULL, LV_ALIGN_IN_BOTTOM_MID, 0, 0);
    lv_kb_set_ta(passkb, pass);
    lv_kb_set_hide_action(passkb, keyboard_hide_action);
    lv_kb_set_ok_action(passkb, keyboard_ok_action);
}

/**
 * Called when the item in wifi list is clicked
 * @param obj pointer to the wifi list
 * @return
 */
static lv_res_t wifiap_list_action(lv_obj_t *obj)
{
    ESP_LOGI(TAG, "[ * ] wifiap list action");
    for (uint8_t i = 0; i < apCount; i++) {
        if (obj == list_ap[i]) {
            current_ap = i;

            /* wifi connect container create */
            wifi_connect_cont = lv_cont_create(lv_scr_act(), NULL);
            lv_obj_set_size(wifi_connect_cont, LV_HOR_RES, LV_VER_RES);
            lv_cont_set_fit(wifi_connect_cont, false, false);

            input_password();

            break;
        }
    }
    return LV_RES_OK;
}

static void wifi_scan_timeout_cb(void *timer)
{
    ESP_LOGI(TAG, "[ * ] wifi scan timeout");

    lv_object_delete(preload);
    lv_object_delete(wifi_list_cont);
}

static void wifi_scan_timer_delete(void)
{
    ESP_LOGI(TAG, "[ * ] wifi scan timer delete");
    if (g_wifi_scan_timer) {
        esp_timer_stop(g_wifi_scan_timer);
        esp_timer_delete(g_wifi_scan_timer);
        g_wifi_scan_timer = NULL;
    }
}

static void wifi_scan_timer_create(void)
{
    ESP_LOGI(TAG, "[ * ] wifi scan timer create");
    esp_timer_create_args_t timer_conf = {
        .callback = wifi_scan_timeout_cb,
        .name = "scan_timeout"
    };

    esp_timer_create(&timer_conf, &g_wifi_scan_timer);

    esp_timer_start_once(g_wifi_scan_timer, WIFI_SCAN_TIMEOUT * 1000U);
}

static void wifi_scan_done(wifi_config_data_t event_data)
{
    wifi_scan_timer_delete();

    lv_object_delete(preload);
    lv_object_delete(scan_label);

    wifilist = lv_list_create(wifi_list_cont, NULL);
    lv_obj_set_size(wifilist, LV_HOR_RES, LV_VER_RES);
    ap_list = (ap_list_t *)event_data.ctx;
    list_ap = (lv_obj_t * *)realloc(list_ap, apCount * sizeof(lv_obj_t *));
    for (uint8_t i = 0; i < apCount; i++) {
        list_ap[i] = lv_list_add(wifilist, SYMBOL_WIFI, (char *)ap_list[i].ssid, wifiap_list_action);
    }
}

static void refresh_wifi_list(void *param)
{
    ESP_LOGI(TAG, "[ * ] refresh wifi list");

    /* wifi list container create */
    wifi_list_cont = lv_cont_create(lv_scr_act(), NULL);
    lv_obj_set_size(wifi_list_cont, LV_HOR_RES, LV_VER_RES);
    lv_cont_set_fit(wifi_list_cont, false, false);

    /* wifi scan label create */
    scan_label = lv_label_create(wifi_list_cont, NULL);
    lv_label_set_text(scan_label, "Scaning...");
    lv_obj_align(scan_label, NULL, LV_ALIGN_CENTER, 0, -80);

    /* wifi scan preload create */
    preload = lv_preload_create(wifi_list_cont, NULL);
    lv_obj_set_size(preload, 80, 80);
    lv_obj_align(preload, NULL, LV_ALIGN_CENTER, 0, 0);
    
    wifi_scan_timer_create();
}

/**
 * Called when the wifi scan is clicked
 * @param obj pointer to the wifi scan btn
 * @return
 */
static lv_res_t wifi_scan_action(lv_obj_t *obj)
{
    ESP_LOGI(TAG, "[ * ] wifi scan");
    if (lv_sw_get_state(obj)) {
        wifi_config_data_t event_data = {0};
        event_data.event = LVGL_WIFI_CONFIG_SCAN;
        xQueueSend(g_event_queue_handle, &event_data, portMAX_DELAY);

        lv_sw_off(obj);
    }
    return LV_RES_OK;
}

static void littlevgl_wificonfig(void)
{
    ESP_LOGI(TAG, "[ * ] littlevgl wificonfig");
    lv_font_add(&password_eye_20, &lv_font_dejavu_20);
    lv_theme_set_current(lv_theme_zen_init(100, NULL));

    lv_obj_t *tabview = lv_tabview_create(lv_scr_act(), NULL);
    lv_obj_set_size(tabview, LV_HOR_RES + 16, LV_VER_RES + 90);
    lv_obj_set_pos(tabview, -8, -60);

    lv_obj_t *tab1 = lv_tabview_add_tab(tabview, SYMBOL_WIFI);
    lv_tabview_set_tab_act(tabview, 0, false);
    lv_obj_set_protect(tabview, LV_PROTECT_PARENT | LV_PROTECT_POS | LV_PROTECT_FOLLOW);
    lv_tabview_set_anim_time(tabview, 0);
    lv_page_set_sb_mode(tab1, LV_SB_MODE_OFF);
    lv_obj_set_protect(tab1, LV_PROTECT_PARENT | LV_PROTECT_POS | LV_PROTECT_FOLLOW);
    lv_page_set_scrl_fit(tab1, false, false);       /* It must not be automatically sized to allow all children to participate. */
    lv_page_set_scrl_height(tab1, LV_VER_RES + 20); /* Set height of the scrollable part of a page */

    /* wifi config container create */
    lv_obj_t *cont = lv_cont_create(tab1, NULL);
    lv_obj_set_size(cont, LV_HOR_RES, LV_VER_RES);
    lv_cont_set_fit(cont, false, false);

    /* wifi config label create */
    lv_obj_t *wifi_config_label = lv_label_create(cont, NULL);
    lv_label_set_text(wifi_config_label, "Wi-Fi Config");
    lv_obj_align(wifi_config_label, cont, LV_ALIGN_IN_TOP_MID, 0, 5); /*Align to LV_ALIGN_IN_TOP_MID*/

    /* wifi scan label create */
    lv_obj_t *scan_sw_label = lv_label_create(cont, NULL);
    lv_label_set_text(scan_sw_label, "Wi-Fi Scan");
    lv_obj_align(scan_sw_label, cont, LV_ALIGN_IN_TOP_MID, -50, 35); /*Align to LV_ALIGN_IN_TOP_MID*/

    /* wifi scan btn create */
    lv_obj_t *scan_sw = lv_sw_create(cont, NULL);
    lv_sw_set_action(scan_sw, wifi_scan_action);
    lv_obj_align(scan_sw, cont, LV_ALIGN_IN_TOP_MID, 50, 30);

    /* wifi connect ap label create */
    lv_obj_t *connectedap_label = lv_label_create(cont, NULL);
    lv_label_set_text(connectedap_label, "Connected AP:");
    lv_obj_align(connectedap_label, cont, LV_ALIGN_IN_TOP_MID, -60, 75); /*Align to LV_ALIGN_IN_TOP_MID*/

    /* wifi connect ap */
    connectedap = lv_label_create(cont, NULL);
    lv_label_set_text(connectedap, "NULL");
    lv_obj_align(connectedap, cont, LV_ALIGN_IN_TOP_MID, 40, 75); /*Align to LV_ALIGN_IN_TOP_MID*/

    /* wifi connect ap rssi label create */
    lv_obj_t *aprssi_label = lv_label_create(cont, NULL);
    lv_label_set_text(aprssi_label, "        AP RSSI:");
    lv_obj_align(aprssi_label, cont, LV_ALIGN_IN_TOP_MID, -60, 115); /*Align to LV_ALIGN_IN_TOP_MID*/

    /* wifi connect ap rssi */
    connectedaprssi = lv_label_create(cont, NULL);
    lv_label_set_text(connectedaprssi, "NULL");
    lv_obj_align(connectedaprssi, cont, LV_ALIGN_IN_TOP_MID, 40, 115); /*Align to LV_ALIGN_IN_TOP_MID*/

    /* wifi connect ap bssid label create */
    lv_obj_t *bssid_label = lv_label_create(cont, NULL);
    lv_label_set_text(bssid_label, "          BSSID:");
    lv_obj_align(bssid_label, cont, LV_ALIGN_IN_TOP_MID, -60, 155); /*Align to LV_ALIGN_IN_TOP_MID*/

    /* wifi connect ap bssid */
    connectedapbssid = lv_label_create(cont, NULL);
    lv_label_set_text(connectedapbssid, "NULL");
    lv_obj_align(connectedapbssid, cont, LV_ALIGN_IN_TOP_MID, 40, 155); /*Align to LV_ALIGN_IN_TOP_MID*/

    /* wifi connect ap channel label create */
    lv_obj_t *channel_label = lv_label_create(cont, NULL);
    lv_label_set_text(channel_label, "      CHANNEL:");
    lv_obj_align(channel_label, cont, LV_ALIGN_IN_TOP_MID, -60, 195); /*Align to LV_ALIGN_IN_TOP_MID*/

    /* wifi connect ap channel */
    connectedapchannel = lv_label_create(cont, NULL);
    lv_label_set_text(connectedapchannel, "NULL");
    lv_obj_align(connectedapchannel, cont, LV_ALIGN_IN_TOP_MID, 40, 195); /*Align to LV_ALIGN_IN_TOP_MID*/
}

static esp_err_t net_event_handler(void *ctx, system_event_t *event)
{
    wifi_mode_t mode;
    static ap_list_t *ap_list = NULL;
    static int s_disconnected_handshake_count = 0;
    static int s_disconnected_unknown_count = 0;

    switch (event->event_id) {
    case SYSTEM_EVENT_STA_START:
        esp_wifi_connect();
        break;
    case SYSTEM_EVENT_STA_GOT_IP: {
        xEventGroupSetBits(wifi_event_group, CONNECTED_BIT);
        esp_wifi_get_mode(&mode);
        wifi_config_data_t event_data = {0};
        event_data.event = LVGL_WIFI_CONFIG_CONNECT_SUCCESS;
        xQueueSend(g_event_queue_handle, &event_data, portMAX_DELAY);
        strcpy((char *)current_apbssid, (char *)event->event_info.connected.bssid);

        wifi_second_chan_t second = 0;
        esp_wifi_get_channel(&current_apchannel, &second);
        break;
    }
    case SYSTEM_EVENT_STA_CONNECTED:
        s_disconnected_unknown_count = 0;
        s_disconnected_handshake_count = 0;
        break;
    case SYSTEM_EVENT_STA_DISCONNECTED: {
        uint8_t sta_conn_state = 0;
        system_event_sta_disconnected_t *disconnected = &event->event_info.disconnected;
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
        xEventGroupClearBits(wifi_event_group, CONNECTED_BIT);
        break;
    }
    case SYSTEM_EVENT_AP_START:
        esp_wifi_get_mode(&mode);
        break;
    case SYSTEM_EVENT_SCAN_DONE: {
        esp_wifi_scan_get_ap_num(&apCount);
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

        ap_list = (ap_list_t *)realloc(ap_list, apCount * sizeof(ap_list_t));
        if (!ap_list) {
            ESP_LOGE(TAG, "[ * ] realloc error, ap_list is NULL");
            break;
        }
        for (int i = 0; i < apCount; ++i) {
            ap_list[i].rssi = wifi_ap_list[i].rssi;
            memcpy(ap_list[i].ssid, wifi_ap_list[i].ssid, sizeof(wifi_ap_list[i].ssid));
        }
        wifi_config_data_t event_data = {0};
        event_data.event = LVGL_WIFI_CONFIG_SCAN_DONE;
        event_data.ctx = ap_list;
        xQueueSend(g_event_queue_handle, &event_data, portMAX_DELAY);
        esp_wifi_scan_stop();
        free(wifi_ap_list);
        break;
    }
    default:
        break;
    }
    return ESP_OK;
}

static void initialise_wifi(void)
{
    tcpip_adapter_init();
    wifi_event_group = xEventGroupCreate();
    ESP_ERROR_CHECK(esp_event_loop_init(net_event_handler, NULL));
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));
    ESP_ERROR_CHECK(esp_wifi_set_storage(WIFI_STORAGE_RAM));
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_start());
}

static void wifi_config_event_cb(void *event)
{
    wifi_config_data_t event_data = {0};
    while (1) {
        if (xQueueReceive(g_event_queue_handle, &event_data, portMAX_DELAY) != pdPASS) {
            continue;
        }

        switch (event_data.event) {
        case LVGL_WIFI_CONFIG_SCAN: {
            wifi_scan_config_t scanConf = {
                .ssid = NULL,
                .bssid = NULL,
                .channel = 0,
                .show_hidden = false
            };
            ESP_ERROR_CHECK(esp_wifi_scan_start(&scanConf, false));

            refresh_wifi_list(NULL);
            break;
        }
        case LVGL_WIFI_CONFIG_SCAN_DONE: {
            ESP_LOGI(TAG, "[ * ] running refresh wifi list：%d\n", apCount);
            wifi_scan_done(event_data);
            break;
        }
        case LVGL_WIFI_CONFIG_CONNECT_SUCCESS:
            wifi_connect_success();
            break;

        case LVGL_WIFI_CONFIG_CONNECT_FAIL:
            wifi_connect_fail();
            break;

        case LVGL_WIFI_CONFIG_TRY_CONNECT: {
            wifi_config_t sta_config = {0};
            strcpy((char *)sta_config.sta.ssid, (char *)ap_list[current_ap].ssid);
            strcpy((char *)sta_config.sta.password, (char *)current_appass);
            ESP_LOGI(TAG, "[ * ] Select SSID：%s", (char *)ap_list[current_ap].ssid);
            ESP_LOGI(TAG, "[ * ] Input Password：%s", (char *)current_appass);
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
    /* Initialize LittlevGL GUI */
    lvgl_init();

    /* wifi config example */
    littlevgl_wificonfig();

    g_event_queue_handle = xQueueCreate(10, sizeof(wifi_config_data_t));
    xTaskCreate(wifi_config_event_cb, "wifi_config_event_cb", 2048, NULL, 4, NULL);

    /* Initialize NVS */
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);
    
    /* initialise wifi */
    initialise_wifi();

    ESP_LOGI(TAG, "[APP] IDF version: %s", esp_get_idf_version());
    ESP_LOGI(TAG, "[APP] Free memory: %d bytes", esp_get_free_heap_size());
}
