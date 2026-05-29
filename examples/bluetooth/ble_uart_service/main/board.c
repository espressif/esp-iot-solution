/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Board support: QSPI display, I2C panel touch, capacitive key pads, emote_gen_player.
 */

#include <inttypes.h>

#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"
#include "driver/spi_common.h"
#include "esp_check.h"
#include "esp_log.h"
#include "esp_lcd_panel_io.h"
#include "esp_lcd_panel_ops.h"
#include "esp_lcd_touch.h"
#include "bsp/display.h"
#include "bsp/esp-bsp.h"
#include "bsp/touch.h"

#include "board.h"

#include "core/gfx_touch.h"
#include "iot_button.h"
#include "touch_button.h"
#include "touch_sensor_lowlevel.h"
#include "touch_slider_sensor.h"

/* =============================================================================
 * State
 * ========================================================================== */

static const char *TAG = "board";

#define BOARD_DISPLAY_FLUSH_LINES      100
#define BOARD_PLAYER_TASK_STACK_SIZE   (5 * 1024)
#define BOARD_KEY_LONG_PRESS_MS        1200
#define BOARD_KEY_SHORT_PRESS_MS       245
#define BOARD_PANEL_TOUCH_POLL_MS      50
#define BOARD_DISPLAY_DEINIT_DELAY_MS  200
#define BOARD_SLIDER_TASK_STACK_SIZE   4096
#define BOARD_SLIDER_POLL_INTERVAL_MS  20
#define BOARD_KEY_BUTTON_MAX           2
#define BOARD_KEY_PAD_ARRAY_COUNT      (sizeof(s_board_key_pads) / sizeof(s_board_key_pads[0]))

static board_touch_cb_t s_board_touch_cb;
static void *s_board_touch_user_data;

static esp_lcd_panel_io_handle_t s_board_panel_io;
static esp_lcd_panel_handle_t s_board_panel;
static bool s_board_spi_bus_inited;

static esp_lcd_touch_handle_t s_board_panel_touch;
static gfx_touch_t *s_board_gfx_touch;

static bool s_board_key_pads_inited;
static bool s_board_key_pads_lowlevel_inited;
static button_handle_t s_board_key_button[BOARD_KEY_BUTTON_MAX];
/** Per-key user_data for iot_button (slot 0 must not be NULL). */
static uint8_t s_board_key_slot[BOARD_KEY_BUTTON_MAX];

#if BOARD_TOUCH_SLIDER_ENABLED
static touch_slider_handle_t s_board_slider;
static TaskHandle_t s_board_slider_task;
static SemaphoreHandle_t s_board_slider_done_sem;
#endif

#if BOARD_TOUCH_SLIDER_ENABLED
static const uint32_t s_board_key_pads[] = { BOARD_TOUCH_PAD_0, BOARD_TOUCH_PAD_1 };
#else
static const uint32_t s_board_key_pads[] = { BOARD_TOUCH_PAD_0 };
#endif

static void board_key_pads_deinit(void);
static void board_panel_touch_deinit(void);
static esp_err_t board_display_init(void);
static void board_display_deinit(void);

/* =============================================================================
 * Public
 * ========================================================================== */

void board_set_touch_callback(board_touch_cb_t cb, void *user_data)
{
    s_board_touch_cb = cb;
    s_board_touch_user_data = user_data;
}

/* =============================================================================
 * emote_gen_player — flush / update
 * ========================================================================== */

static void board_player_flush_cb(int x1, int y1, int x2, int y2, const void *data, emote_gen_player_handle_t manager)
{
    (void)manager;
    esp_lcd_panel_draw_bitmap(s_board_panel, x1, y1, x2, y2, data);
}

static void board_player_update_cb(gfx_disp_event_t event, const void *obj, emote_gen_player_handle_t manager)
{
    (void)manager;
    (void)obj;
    if (event == GFX_DISP_EVENT_PART_FRAME_DONE) {
        ESP_LOGD(TAG, "emote part done (%p) ev:%d", obj, event);
    } else if (event == GFX_DISP_EVENT_ALL_FRAME_DONE) {
        ESP_LOGD(TAG, "emote all done (%p) ev:%d", obj, event);
    }
}

/* =============================================================================
 * User touch (key pads + panel)
 * ========================================================================== */

static void board_dispatch_touch_event(const board_touch_event_t *touch_event)
{
    if (s_board_touch_cb != NULL) {
        s_board_touch_cb(s_board_touch_user_data, touch_event);
    }
}

static const char *board_button_event_name(button_event_t ev)
{
    switch (ev) {
    case BUTTON_PRESS_DOWN: return "PRESS_DOWN";
    case BUTTON_PRESS_UP: return "PRESS_UP";
    case BUTTON_PRESS_REPEAT: return "PRESS_REPEAT";
    case BUTTON_PRESS_REPEAT_DONE: return "PRESS_REPEAT_DONE";
    case BUTTON_SINGLE_CLICK: return "SINGLE_CLICK";
    case BUTTON_DOUBLE_CLICK: return "DOUBLE_CLICK";
    case BUTTON_MULTIPLE_CLICK: return "MULTIPLE_CLICK";
    case BUTTON_LONG_PRESS_START: return "LONG_PRESS_START";
    case BUTTON_LONG_PRESS_HOLD: return "LONG_PRESS_HOLD";
    case BUTTON_LONG_PRESS_UP: return "LONG_PRESS_UP";
    case BUTTON_PRESS_END: return "PRESS_END";
    case BUTTON_EVENT_MAX: return "EVENT_MAX";
    case BUTTON_NONE_PRESS: return "NONE_PRESS";
    default: return "UNKNOWN";
    }
}

static void board_key_button_event_cb(void *button_handle, void *usr_data)
{
    int8_t key_index = -1;
    if (usr_data != NULL) {
        key_index = (int8_t)(*(const uint8_t *)usr_data);
    } else {
        for (size_t b = 0; b < BOARD_KEY_PAD_ARRAY_COUNT; b++) {
            if (s_board_key_button[b] == (button_handle_t)button_handle) {
                key_index = (int8_t)b;
                break;
            }
        }
    }
    button_event_t ev = iot_button_get_event((button_handle_t)button_handle);
    const board_touch_event_t touch_event = {
        .src   = BOARD_TOUCH_SOURCE_KEY,
        .code  = (int)ev,
        .key_index = key_index,
    };
    ESP_LOGD(TAG, "key[%d] %s (%d)", (int)touch_event.key_index, board_button_event_name(ev), (int)ev);
    board_dispatch_touch_event(&touch_event);
}

static void board_panel_touch_event_cb(gfx_touch_t *touch, const gfx_touch_event_t *ev, void *user_data)
{
    (void)touch;
    (void)user_data;
    if (ev == NULL) {
        return;
    }
    const board_touch_event_t touch_event = {
        .src   = BOARD_TOUCH_SOURCE_PANEL,
        .code  = (int)ev->type,
        .key_index = -1,
    };
    switch (ev->type) {
    case GFX_TOUCH_EVENT_PRESS:
        ESP_LOGD(TAG, "panel: press (%u, %u)", ev->x, ev->y);
        break;
    case GFX_TOUCH_EVENT_RELEASE:
        ESP_LOGD(TAG, "panel: release (%u, %u)", ev->x, ev->y);
        break;
    case GFX_TOUCH_EVENT_MOVE:
        ESP_LOGD(TAG, "panel: move (%u, %u)", ev->x, ev->y);
        break;
    default:
        ESP_LOGD(TAG, "panel: type %d", (int)ev->type);
        break;
    }
    board_dispatch_touch_event(&touch_event);
}

/* =============================================================================
 * Key pads: optional slider + iot_button
 * ========================================================================== */

#if BOARD_TOUCH_SLIDER_ENABLED
static void board_slider_event_cb(touch_slider_handle_t handle, touch_slider_event_t event, int32_t data, void *arg)
{
    (void)handle;
    (void)data;
    (void)arg;
    switch (event) {
    case TOUCH_SLIDER_EVENT_RIGHT_SWIPE:
        ESP_LOGI(TAG, "slider: right swipe");
        break;
    case TOUCH_SLIDER_EVENT_LEFT_SWIPE:
        ESP_LOGI(TAG, "slider: left swipe");
        break;
    case TOUCH_SLIDER_EVENT_RELEASE:
        ESP_LOGD(TAG, "slider: release");
        break;
    default:
        break;
    }
}

static void board_slider_worker_task(void *p)
{
    touch_slider_handle_t h = (touch_slider_handle_t)p;
    for (;;) {
        if (touch_slider_sensor_handle_events(h) != ESP_OK) {
            ESP_LOGE(TAG, "slider poll failed");
        }
        if (ulTaskNotifyTake(pdTRUE, pdMS_TO_TICKS(BOARD_SLIDER_POLL_INTERVAL_MS)) > 0) {
            break;
        }
    }
    if (s_board_slider_done_sem != NULL) {
        xSemaphoreGive(s_board_slider_done_sem);
    }
    vTaskDelete(NULL);
}
#endif

static void board_key_pads_deinit(void)
{
#if BOARD_TOUCH_SLIDER_ENABLED
    if (s_board_slider_task != NULL) {
        xTaskNotifyGive(s_board_slider_task);
        if (s_board_slider_done_sem != NULL) {
            (void)xSemaphoreTake(s_board_slider_done_sem, portMAX_DELAY);
        }
        s_board_slider_task = NULL;
    }
    if (s_board_slider != NULL) {
        touch_slider_sensor_delete(s_board_slider);
        s_board_slider = NULL;
    }
    if (s_board_slider_done_sem != NULL) {
        vSemaphoreDelete(s_board_slider_done_sem);
        s_board_slider_done_sem = NULL;
    }
#endif
    for (size_t i = 0; i < BOARD_KEY_PAD_ARRAY_COUNT; i++) {
        if (s_board_key_button[i] != NULL) {
            iot_button_delete(s_board_key_button[i]);
            s_board_key_button[i] = NULL;
        }
    }
    if (s_board_key_pads_lowlevel_inited) {
        (void)touch_sensor_lowlevel_stop();
        (void)touch_sensor_lowlevel_delete();
        s_board_key_pads_lowlevel_inited = false;
    }
    s_board_key_pads_inited = false;
}

/** iot_button uses periodic timer. No key logs: try @ref BOARD_TOUCH_SLIDER_ENABLED, @ref BOARD_TOUCH_BUTTON_THRESHOLD, pad numbers, @c CONFIG_BUTTON_PERIOD_TIME_MS. */
static esp_err_t board_key_pads_init(void)
{
    s_board_key_pads_inited = false;

    touch_lowlevel_type_t types[BOARD_KEY_PAD_ARRAY_COUNT];
    for (size_t i = 0; i < BOARD_KEY_PAD_ARRAY_COUNT; i++) {
        types[i] = TOUCH_LOWLEVEL_TYPE_TOUCH;
    }
    touch_lowlevel_config_t low = {
        .channel_num = BOARD_KEY_PAD_ARRAY_COUNT,
        .channel_list = (uint32_t *)s_board_key_pads,
        .channel_type = types,
    };

    esp_err_t err = touch_sensor_lowlevel_create(&low);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "pad lowlevel create: %s", esp_err_to_name(err));
        return err;
    }
    s_board_key_pads_lowlevel_inited = true;

    const button_config_t iot = {
        .long_press_time = BOARD_KEY_LONG_PRESS_MS,
        .short_press_time = BOARD_KEY_SHORT_PRESS_MS,
    };

    for (size_t i = 0; i < BOARD_KEY_PAD_ARRAY_COUNT; i++) {
        s_board_key_slot[i] = (uint8_t)i;
        button_touch_config_t t = {
            .touch_channel = (int32_t)s_board_key_pads[i],
            .channel_threshold = BOARD_TOUCH_BUTTON_THRESHOLD,
            .skip_lowlevel_init = true,
        };
        ESP_LOGI(TAG, "key %u, pad %" PRIu32, (unsigned)(i + 1), s_board_key_pads[i]);
        err = iot_button_new_touch_button_device(&iot, &t, &s_board_key_button[i]);
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "iot_button_new_touch_button_device: %s", esp_err_to_name(err));
            board_key_pads_deinit();
            return err;
        }
    }

    err = touch_sensor_lowlevel_start();
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "pad lowlevel start: %s", esp_err_to_name(err));
        board_key_pads_deinit();
        return err;
    }

    /* touch_button does not support every iot_button event; invalid ones fail registration. */
    static const button_event_t evs[] = {
        BUTTON_PRESS_UP,
        BUTTON_PRESS_DOWN,
        BUTTON_SINGLE_CLICK,
        BUTTON_LONG_PRESS_START,
    };
    for (size_t b = 0; b < BOARD_KEY_PAD_ARRAY_COUNT; b++) {
        ESP_LOGI(TAG, "register key[%u] @%p", (unsigned)b, (void *)s_board_key_button[b]);
        for (size_t e = 0; e < sizeof(evs) / sizeof(evs[0]); e++) {
            err = iot_button_register_cb(s_board_key_button[b], evs[e], NULL, board_key_button_event_cb, &s_board_key_slot[b]);
            if (err != ESP_OK) {
                ESP_LOGE(TAG, "iot_button_register_cb ch%u ev%d: %s", (unsigned)b, (int)evs[e], esp_err_to_name(err));
                board_key_pads_deinit();
                return err;
            }
        }
    }
    (void)iot_button_resume();

#if BOARD_TOUCH_SLIDER_ENABLED
    if (BOARD_KEY_PAD_ARRAY_COUNT >= 2) {
        s_board_slider_done_sem = xSemaphoreCreateBinary();
        if (s_board_slider_done_sem == NULL) {
            ESP_LOGE(TAG, "xSemaphoreCreateBinary(slider_done) failed");
            board_key_pads_deinit();
            return ESP_FAIL;
        }
        float th[] = { 0.015f, 0.015f };
        touch_slider_config_t sc = {
            .channel_num = BOARD_KEY_PAD_ARRAY_COUNT,
            .channel_list = (uint32_t *)s_board_key_pads,
            .channel_threshold = th,
            .channel_gold_value = NULL,
            .debounce_times = 1,
            .filter_reset_times = 2,
            .position_range = 100,
            .calculate_window = 2,
            .swipe_threshold = 4.0f,
            .swipe_hysterisis = 2.0f,
            .swipe_alpha = 0.3f,
            .skip_lowlevel_init = true,
        };
        err = touch_slider_sensor_create(&sc, &s_board_slider, board_slider_event_cb, NULL);
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "slider create: %s", esp_err_to_name(err));
            board_key_pads_deinit();
            return err;
        }
        if (xTaskCreate(board_slider_worker_task, "touch_slider", BOARD_SLIDER_TASK_STACK_SIZE, s_board_slider, 5,
                        &s_board_slider_task) != pdPASS) {
            ESP_LOGE(TAG, "xTaskCreate(slider) failed");
            (void)touch_slider_sensor_delete(s_board_slider);
            s_board_slider = NULL;
            vSemaphoreDelete(s_board_slider_done_sem);
            s_board_slider_done_sem = NULL;
            board_key_pads_deinit();
            return ESP_FAIL;
        }
    }
#endif
    s_board_key_pads_inited = true;
    return ESP_OK;
}

/* =============================================================================
 * Display: QSPI panel + SPI bus teardown
 * ========================================================================== */

static bool board_spi_flush_done_cb(esp_lcd_panel_io_handle_t io, esp_lcd_panel_io_event_data_t *edata, void *user_ctx)
{
    (void)io;
    (void)edata;
    emote_gen_player_handle_t h = (emote_gen_player_handle_t)user_ctx;
    if (h) {
        emote_gen_player_notify_flush_finished(h);
    }
    return true;
}

static void board_display_clear_flush_callback(void)
{
    if (s_board_panel_io != NULL) {
        const esp_lcd_panel_io_callbacks_t spi_cbs = { 0 };
        (void)esp_lcd_panel_io_register_event_callbacks(s_board_panel_io, &spi_cbs, NULL);
    }
}

static esp_err_t board_display_init(void)
{
    const bsp_display_config_t cfg = {
        .max_transfer_sz = (BSP_LCD_H_RES * BOARD_DISPLAY_FLUSH_LINES) * sizeof(uint16_t),
    };
    esp_err_t err = bsp_display_new(&cfg, &s_board_panel, &s_board_panel_io);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "bsp_display_new: %s", esp_err_to_name(err));
        s_board_panel = NULL;
        s_board_panel_io = NULL;
        return err;
    }
    s_board_spi_bus_inited = true;

    err = esp_lcd_panel_disp_on_off(s_board_panel, true);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "panel on: %s", esp_err_to_name(err));
        board_display_deinit();
        return err;
    }

    err = bsp_display_backlight_on();
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "backlight on: %s", esp_err_to_name(err));
        board_display_deinit();
        return err;
    }

    return ESP_OK;
}

static void board_display_deinit(void)
{
    const bool had_spi_bus = s_board_spi_bus_inited;

    if (s_board_panel != NULL) {
        esp_lcd_panel_del(s_board_panel);
        s_board_panel = NULL;
    }
    if (s_board_panel_io != NULL) {
        esp_lcd_panel_io_del(s_board_panel_io);
        s_board_panel_io = NULL;
    }
    if (had_spi_bus) {
        spi_bus_free(BSP_LCD_SPI_NUM);
        s_board_spi_bus_inited = false;
    }
    vTaskDelay(pdMS_TO_TICKS(BOARD_DISPLAY_DEINIT_DELAY_MS));
}

/* Before emote_gen_player_deinit — removes gfx touch node, CST816S, I2C. */
static void board_panel_touch_deinit(void)
{
    if (s_board_gfx_touch != NULL) {
        gfx_touch_del(s_board_gfx_touch);
        s_board_gfx_touch = NULL;
    }
    const bool had = (s_board_panel_touch != NULL);
    if (s_board_panel_touch != NULL) {
        esp_lcd_touch_del(s_board_panel_touch);
        s_board_panel_touch = NULL;
    }
    if (had) {
        bsp_i2c_deinit();
    }
}

/* =============================================================================
 * App lifecycle
 * ========================================================================== */

esp_err_t board_start(board_app_t *app)
{
    static const char *const asset_partition_label = "emote_gen";
    emote_gen_player_config_t pcfg = {
        .flags = {
            .swap = true,
            .double_buffer = true,
            .buff_dma = true,
            .buff_spiram = false,
        },
        .gfx_emote = { .h_res = BSP_LCD_H_RES, .v_res = BSP_LCD_V_RES, .fps = 30 },
        .buffers = { .buf_pixels = BSP_LCD_H_RES * 16 },
        .task = {
            .task_priority = 7,
            .task_stack = BOARD_PLAYER_TASK_STACK_SIZE,
            .task_affinity = 0,
            .task_stack_in_ext = false,
        },
        .flush_cb = board_player_flush_cb,
        .update_cb = board_player_update_cb,
    };
    emote_gen_player_data_t assets = {
        .type = EMOTE_GEN_PLAYER_SOURCE_PARTITION,
        .source = { .partition_label = asset_partition_label },
        .flags = { .mmap_enable = 1 },
    };

    ESP_RETURN_ON_FALSE(app != NULL, ESP_ERR_INVALID_ARG, TAG, "app");

    app->player = NULL;
    esp_err_t err = board_display_init();
    if (err != ESP_OK) {
        return err;
    }

    err = board_key_pads_init();
    if (err != ESP_OK) {
        board_display_deinit();
        return err;
    }

    app->player = emote_gen_player_init(&pcfg);
    if (app->player == NULL) {
        board_key_pads_deinit();
        board_display_deinit();
        return ESP_FAIL;
    }
    if (emote_gen_player_get_disp(app->player) == NULL) {
        ESP_LOGE(TAG, "emote disp is NULL");
        board_stop(app);
        return ESP_FAIL;
    }
    if (emote_gen_player_get_gfx_handle(app->player) == NULL) {
        ESP_LOGE(TAG, "emote gfx handle is NULL");
        board_stop(app);
        return ESP_FAIL;
    }

    const esp_lcd_panel_io_callbacks_t spi_cbs = { .on_color_trans_done = board_spi_flush_done_cb };
    esp_lcd_panel_io_register_event_callbacks(s_board_panel_io, &spi_cbs, app->player);

    gfx_handle_t gh = emote_gen_player_get_gfx_handle(app->player);
    gfx_disp_t *dp = emote_gen_player_get_disp(app->player);

    err = bsp_touch_new(NULL, &s_board_panel_touch);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "bsp_touch_new: %s", esp_err_to_name(err));
        s_board_panel_touch = NULL;
        board_stop(app);
        return err;
    }

    gfx_touch_config_t tcfg = {
        .handle = s_board_panel_touch,
        .event_cb = board_panel_touch_event_cb,
        .disp = dp,
        .poll_ms = BOARD_PANEL_TOUCH_POLL_MS,
        .user_data = gh,
    };
    s_board_gfx_touch = gfx_touch_add(gh, &tcfg);
    if (s_board_gfx_touch == NULL) {
        ESP_LOGE(TAG, "gfx_touch_add (panel) failed");
        board_stop(app);
        return ESP_FAIL;
    }

    err = emote_gen_player_mount_assets(app->player, &assets);
    if (err != ESP_OK) {
        board_stop(app);
        return err;
    }

    ESP_LOGI(TAG, "started player %p (keys + panel)", (void *)app->player);
    return ESP_OK;
}

void board_stop(board_app_t *app)
{
    if (app == NULL) {
        return;
    }
    board_set_touch_callback(NULL, NULL);
    board_panel_touch_deinit();
    board_display_clear_flush_callback();
    if (app->player != NULL) {
        emote_gen_player_deinit(app->player);
        app->player = NULL;
    }
    board_key_pads_deinit();
    board_display_deinit();
}
