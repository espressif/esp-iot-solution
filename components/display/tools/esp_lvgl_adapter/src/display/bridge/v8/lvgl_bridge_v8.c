/*
 * SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/*********************
 *      INCLUDES
 *********************/
#include <stdlib.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>
#include "sdkconfig.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "soc/soc_caps.h"
#include "esp_log.h"
#include "esp_check.h"
#include "esp_lcd_panel_io.h"
#include "esp_lcd_panel_ops.h"
#include "esp_cache.h"
#include "esp_private/esp_cache_private.h"
#include "esp_heap_caps.h"
#include "display_bridge.h"
#include "display_manager.h"
#include "lvgl_port_ppa.h"
#include "display_te_sync.h"
#include "lvgl_port_alignment.h"
#include "common/display_bridge_common.h"

#if SOC_MIPI_DSI_SUPPORTED
#include "esp_lcd_mipi_dsi.h"
#endif

#if SOC_LCDCAM_RGB_LCD_SUPPORTED
#include "esp_lcd_panel_rgb.h"
#endif

#define ESP_LV_ADAPTER_DUMMY_DRAW_EVT_COLOR_DONE  (1U << 0)
#define ESP_LV_ADAPTER_DUMMY_DRAW_EVT_FRAME_DONE  (1U << 1)

#if CONFIG_SOC_PPA_SUPPORTED
#include "driver/ppa.h"
#endif

#if SOC_DMA2D_SUPPORTED
#include "esp_async_fbcpy.h"
#endif

/*********************
 *      DEFINES
 *********************/
/* Color depth constants */
#define COLOR_DEPTH_RGB565         (16)    /* 16-bit RGB565 color format */
#define COLOR_DEPTH_RGB888         (24)    /* 24-bit RGB888 color format */
#define COLOR_BYTES_RGB565         (2)     /* Bytes per pixel for RGB565 */
#define COLOR_BYTES_RGB888         (3)     /* Bytes per pixel for RGB888 */

/* PPA transformation constants */
#define PPA_SCALE_FACTOR_NO_SCALE  (1.0f)  /* No scaling (1:1) */
#define PPA_SWAP_DISABLED          (0)     /* RGB/byte swap disabled */

/* Hardware alignment */
#define PPA_DEFAULT_ALIGNMENT      (128)   /* Default PPA alignment in bytes */

/*********************
 *      TYPEDEFS
 *********************/
#if LVGL_VERSION_MAJOR >= 9

/* Empty implementation for LVGL v9+ */

#else

typedef struct esp_lv_adapter_display_bridge_v8 {
    esp_lv_adapter_display_bridge_t base;
    esp_lv_adapter_display_runtime_config_t cfg;
    esp_lcd_panel_handle_t panel;
    esp_lcd_panel_io_handle_t panel_io;
    esp_lv_adapter_display_runtime_info_t runtime;
    esp_lv_adapter_display_dirty_region_t dirty;
    void *front_fb;
    void *back_fb;
    void *spare_fb;
    void *rgb_last_buf;
    void *rgb_next_buf;
    void *rgb_flush_next_buf;
    void *toggle_fb;
    TaskHandle_t notify_task;
    TaskHandle_t dummy_draw_wait_task;
    uint32_t dummy_draw_wait_mask;
    bool dummy_draw;
    int block_size_small;
    int block_size_large;
    size_t cache_line_size;
} esp_lv_adapter_display_bridge_v8_t;

#if CONFIG_SOC_PPA_SUPPORTED || CONFIG_SOC_DMA2D_SUPPORTED
typedef struct {
    size_t data_cache_line_size;
#if CONFIG_SOC_PPA_SUPPORTED
    ppa_client_handle_t ppa_handle;
#endif
#if CONFIG_SOC_DMA2D_SUPPORTED
    esp_async_fbcpy_handle_t fbcpy_handle;
    SemaphoreHandle_t dma2d_mutex;
    SemaphoreHandle_t dma2d_done_sem;
#endif
} display_bridge_v8_hw_resource_t;
#endif

#endif /* LVGL_VERSION_MAJOR >= 9 */

/**********************
 *  STATIC VARIABLES
 **********************/
static const char *TAG = "esp_lvgl:bridge_v8";

#if LVGL_VERSION_MAJOR < 9
#if CONFIG_SOC_PPA_SUPPORTED || CONFIG_SOC_DMA2D_SUPPORTED
static display_bridge_v8_hw_resource_t hw_resource = {0};
#endif
#endif

/**********************
 *  INLINE FUNCTIONS
 **********************/
#if LVGL_VERSION_MAJOR < 9

static inline uint16_t bridge_h_res(const esp_lv_adapter_display_bridge_v8_t *impl)
{
    return impl->runtime.hor_res;
}

static inline uint16_t bridge_v_res(const esp_lv_adapter_display_bridge_v8_t *impl)
{
    return impl->runtime.ver_res;
}

static inline esp_lv_adapter_rotation_t bridge_rotation(const esp_lv_adapter_display_bridge_v8_t *impl)
{
    return impl->runtime.rotation;
}

static inline uint8_t bridge_color_bytes(const esp_lv_adapter_display_bridge_v8_t *impl)
{
    return impl->runtime.color_bytes;
}

static size_t display_bridge_v8_i1_stride_bytes(uint16_t hor_res)
{
    uint16_t aligned = (hor_res + 7U) & ~7U;
    return (size_t)aligned / 8U;
}

static bool display_bridge_v8_prepare_mono(esp_lv_adapter_display_bridge_v8_t *impl,
                                           lv_disp_drv_t *drv,
                                           const lv_area_t *area,
                                           uint8_t **color_map)
{
    if (!impl || !drv || !area || !color_map || !*color_map) {
        return false;
    }

    if (impl->cfg.base.profile.mono_layout == ESP_LV_ADAPTER_MONO_LAYOUT_NONE) {
        return true;
    }

    if (LV_COLOR_DEPTH != 1) {
        ESP_LOGE(TAG, "Monochrome requires LV_COLOR_DEPTH=1");
        return false;
    }

    const esp_lv_adapter_rotation_t rotation = impl->cfg.base.profile.rotation;
    uint16_t phy_w = impl->cfg.base.profile.hor_res;
    uint16_t phy_h = impl->cfg.base.profile.ver_res;
    uint16_t log_w = (rotation == ESP_LV_ADAPTER_ROTATE_90 || rotation == ESP_LV_ADAPTER_ROTATE_270) ? phy_h : phy_w;
    size_t dst_stride = display_bridge_v8_i1_stride_bytes(phy_w);

    if (impl->cfg.base.profile.mono_layout == ESP_LV_ADAPTER_MONO_LAYOUT_HTILED) {
        if ((area->x1 % 8) != 0 || ((area->x2 + 1) % 8) != 0) {
            ESP_LOGW(TAG, "I1 htiled area not byte-aligned (x1=%d x2=%d)", area->x1, area->x2);
        }
        if (impl->cfg.base.profile.rotation == ESP_LV_ADAPTER_ROTATE_0) {
            *color_map += 8;
            return true;
        }
        if (!impl->cfg.mono_buf) {
            ESP_LOGE(TAG, "Monochrome HTILED buffer missing");
            return false;
        }
    } else {
        if (!impl->cfg.mono_buf) {
            ESP_LOGE(TAG, "Monochrome VTILED buffer missing");
            return false;
        }
    }

    size_t buf_size = (size_t)phy_w * phy_h / 8;
    memset(impl->cfg.mono_buf, 0x00, buf_size);

    /* V8 uses 1 byte per pixel (different from V9's packed I1 format) */
    uint8_t *src = *color_map;
    size_t src_stride_px = log_w;

    for (int y = area->y1; y <= area->y2; y++) {
        for (int x = area->x1; x <= area->x2; x++) {
            bool pixel = src[y * src_stride_px + x] != 0;
            int px, py;
            display_coord_to_phy(x, y, &px, &py, rotation, phy_w, phy_h);

            size_t offset;
            if (impl->cfg.base.profile.mono_layout == ESP_LV_ADAPTER_MONO_LAYOUT_HTILED) {
                offset = dst_stride * (size_t)py + (size_t)(px >> 3);
                if (offset < buf_size) {
                    uint8_t mask = (uint8_t)(1U << (7 - (px & 7)));
                    if (pixel) {
                        impl->cfg.mono_buf[offset] |= mask;
                    } else {
                        impl->cfg.mono_buf[offset] &= ~mask;
                    }
                }
            } else {
                offset = (size_t)phy_w * (size_t)(py >> 3) + (size_t)px;
                if (offset < buf_size) {
                    uint8_t mask = (uint8_t)(1U << (py & 7));
                    if (pixel) {
                        impl->cfg.mono_buf[offset] &= ~mask;
                    } else {
                        impl->cfg.mono_buf[offset] |= mask;
                    }
                }
            }
        }
    }
    *color_map = impl->cfg.mono_buf;
    return true;
}

#endif /* LVGL_VERSION_MAJOR < 9 */

/**********************
 *  FORWARD DECLARATIONS
 **********************/
#if LVGL_VERSION_MAJOR < 9

/* Lifecycle management */
static void display_bridge_v8_destroy(esp_lv_adapter_display_bridge_t *bridge);
static esp_err_t display_bridge_v8_update_panel(esp_lv_adapter_display_bridge_t *bridge,
                                                const esp_lv_adapter_display_runtime_config_t *cfg);

static void display_bridge_v8_set_area_rounder(esp_lv_adapter_display_bridge_t *bridge,
                                               void (*rounder_cb)(lv_area_t *, void *),
                                               void *user_data);

/* Core callbacks */
static void display_bridge_v8_flush_entry(esp_lv_adapter_display_bridge_t *bridge,
                                          void *disp_ref,
                                          const lv_area_t *area,
                                          uint8_t *color_map);
static void display_bridge_v8_set_dummy_draw(esp_lv_adapter_display_bridge_t *bridge, bool enable);
static void display_bridge_v8_set_dummy_draw_callbacks(esp_lv_adapter_display_bridge_t *bridge,
                                                       const esp_lv_adapter_dummy_draw_callbacks_t *cbs,
                                                       void *user_ctx);
static esp_err_t display_bridge_v8_dummy_draw_blit(esp_lv_adapter_display_bridge_t *bridge,
                                                   int x_start,
                                                   int y_start,
                                                   int x_end,
                                                   int y_end,
                                                   const void *frame_buffer,
                                                   bool wait);

/* VSync handling */
static bool display_bridge_v8_handle_vsync(esp_lv_adapter_display_bridge_v8_t *impl);
static void display_bridge_v8_register_vsync(esp_lv_adapter_display_bridge_v8_t *impl);
static inline void display_bridge_v8_signal_dummy_draw_event(esp_lv_adapter_display_bridge_v8_t *impl,
                                                             uint32_t event_bit,
                                                             BaseType_t *need_yield);

#if CONFIG_SOC_MIPI_DSI_SUPPORTED
static bool display_bridge_v8_on_mipi_color_trans_done(esp_lcd_panel_handle_t panel,
                                                       esp_lcd_dpi_panel_event_data_t *event_data,
                                                       void *user_ctx);
static bool display_bridge_v8_on_mipi_refresh_done(esp_lcd_panel_handle_t panel,
                                                   esp_lcd_dpi_panel_event_data_t *event_data,
                                                   void *user_ctx);
#endif

#if CONFIG_SOC_LCD_RGB_SUPPORTED
static bool display_bridge_v8_on_rgb_color_trans_done(esp_lcd_panel_handle_t panel,
                                                      const esp_lcd_rgb_panel_event_data_t *event_data,
                                                      void *user_ctx);
static bool display_bridge_v8_on_rgb_frame_complete(esp_lcd_panel_handle_t panel,
                                                    const esp_lcd_rgb_panel_event_data_t *event_data,
                                                    void *user_ctx);
#endif

static bool display_bridge_v8_on_io_color_trans_done(esp_lcd_panel_io_handle_t panel_io,
                                                     esp_lcd_panel_io_event_data_t *edata,
                                                     void *user_ctx);

/* Flush implementations */
static void display_bridge_v8_flush_default(esp_lv_adapter_display_bridge_v8_t *impl,
                                            lv_disp_drv_t *drv,
                                            const lv_area_t *area,
                                            uint8_t *color_map);
static void display_bridge_v8_flush_gpio_te(esp_lv_adapter_display_bridge_v8_t *impl,
                                            lv_disp_drv_t *drv,
                                            const lv_area_t *area,
                                            uint8_t *color_map);

static void display_bridge_v8_flush_double_full(esp_lv_adapter_display_bridge_v8_t *impl,
                                                lv_disp_drv_t *drv,
                                                const lv_area_t *area,
                                                uint8_t *color_map);

static void display_bridge_v8_flush_triple_full(esp_lv_adapter_display_bridge_v8_t *impl,
                                                lv_disp_drv_t *drv,
                                                const lv_area_t *area,
                                                uint8_t *color_map);

static void display_bridge_v8_flush_double_direct(esp_lv_adapter_display_bridge_v8_t *impl,
                                                  lv_disp_drv_t *drv,
                                                  const lv_area_t *area,
                                                  uint8_t *color_map);

static void display_bridge_v8_flush_triple_diff(esp_lv_adapter_display_bridge_v8_t *impl,
                                                lv_disp_drv_t *drv,
                                                const lv_area_t *area,
                                                uint8_t *color_map);

static void display_bridge_v8_flush_direct_rotate(esp_lv_adapter_display_bridge_v8_t *impl,
                                                  lv_disp_drv_t *drv,
                                                  const lv_area_t *area,
                                                  uint8_t *color_map);

static void display_bridge_v8_flush_full_rotate(esp_lv_adapter_display_bridge_v8_t *impl,
                                                lv_disp_drv_t *drv,
                                                const lv_area_t *area,
                                                uint8_t *color_map);

static void display_bridge_v8_flush_partial_rotate(esp_lv_adapter_display_bridge_v8_t *impl,
                                                   lv_disp_drv_t *drv,
                                                   const lv_area_t *area,
                                                   uint8_t *color_map);

static void rotate_copy_strided_region(const void *src,
                                       void *dst_fb,
                                       uint16_t lv_x_start,
                                       uint16_t lv_y_start,
                                       uint16_t lv_x_end,
                                       uint16_t lv_y_end,
                                       uint16_t src_stride_px,
                                       esp_lv_adapter_display_bridge_v8_t *impl);

static void rotate_copy_region(esp_lv_adapter_display_bridge_v8_t *impl,
                               const void *from,
                               void *to,
                               uint16_t x_start,
                               uint16_t y_start,
                               uint16_t x_end,
                               uint16_t y_end,
                               uint16_t w,
                               uint16_t h,
                               esp_lv_adapter_rotation_t rotation,
                               uint8_t color_bytes);

/* Helper functions */
static void copy_unrendered_area_from_front_to_back(lv_disp_t *disp_refr,
                                                    esp_lv_adapter_display_bridge_v8_t *impl);

static void flush_dirty_save(esp_lv_adapter_display_dirty_region_t *dirty_area);

static esp_lv_adapter_display_flush_probe_t flush_copy_probe(lv_disp_drv_t *drv,
                                                             lv_disp_t *disp);

static void flush_dirty_copy(esp_lv_adapter_display_bridge_v8_t *impl,
                             void *dst,
                             void *src,
                             esp_lv_adapter_display_dirty_region_t *dirty_area);

#endif /* LVGL_VERSION_MAJOR < 9 */

/**********************
 *   PUBLIC API
 **********************/

/**
 * @brief Create LVGL v8 display bridge
 */
esp_lv_adapter_display_bridge_t *esp_lv_adapter_display_bridge_v8_create(const esp_lv_adapter_display_runtime_config_t *cfg)
{
#if LVGL_VERSION_MAJOR >= 9
    ESP_LOGE(TAG, "LVGL v8 bridge requires LVGL_VERSION_MAJOR < 9");
    (void)cfg;
    return NULL;
#else
    esp_lv_adapter_display_bridge_v8_t *impl = calloc(1, sizeof(*impl));
    if (!impl) {
        ESP_LOGE(TAG, "alloc bridge failed");
        return NULL;
    }

#if CONFIG_SOC_DMA2D_SUPPORTED || CONFIG_SOC_PPA_SUPPORTED
    static bool hw_resource_initialized = false;
    if (!hw_resource_initialized) {
        ESP_LOGI(TAG, "Initializing hardware resources (v8)");

        ESP_ERROR_CHECK(esp_cache_get_alignment(MALLOC_CAP_SPIRAM, &hw_resource.data_cache_line_size));
        if (hw_resource.data_cache_line_size == 0) {
            hw_resource.data_cache_line_size = PPA_DEFAULT_ALIGNMENT;
        }

#if CONFIG_SOC_DMA2D_SUPPORTED
        esp_async_fbcpy_config_t cfg_dma = { };
        ESP_ERROR_CHECK(esp_async_fbcpy_install(&cfg_dma, &hw_resource.fbcpy_handle));

        hw_resource.dma2d_mutex = xSemaphoreCreateMutex();
        hw_resource.dma2d_done_sem = xSemaphoreCreateBinary();
        assert(hw_resource.dma2d_mutex && hw_resource.dma2d_done_sem);
#endif

        hw_resource_initialized = true;
        ESP_LOGI(TAG, "Hardware resources initialized (v8)");
    }
#endif

#if CONFIG_SOC_PPA_SUPPORTED
    if (hw_resource.ppa_handle == NULL) {
        ppa_client_config_t ppa_srm_config = {
            .oper_type = PPA_OPERATION_SRM,
            .max_pending_trans_num = LVGL_PORT_PPA_MAX_PENDING_TRANS,
        };
        ESP_ERROR_CHECK(ppa_register_client(&ppa_srm_config, &hw_resource.ppa_handle));
    }
#endif

    impl->base.flush = display_bridge_v8_flush_entry;
    impl->base.destroy = display_bridge_v8_destroy;
    impl->base.set_dummy_draw = display_bridge_v8_set_dummy_draw;
    impl->base.set_dummy_draw_callbacks = display_bridge_v8_set_dummy_draw_callbacks;
    impl->base.dummy_draw_blit = display_bridge_v8_dummy_draw_blit;
    impl->base.update_panel = display_bridge_v8_update_panel;
    impl->base.set_area_rounder = display_bridge_v8_set_area_rounder;
    impl->cfg = *cfg;
    impl->panel = cfg->base.panel;
    impl->dummy_draw = cfg->dummy_draw_enabled;
    impl->notify_task = NULL;
    impl->toggle_fb = NULL;
    impl->dummy_draw_wait_task = NULL;
    impl->dummy_draw_wait_mask = 0;
    impl->cache_line_size = display_bridge_get_cache_line_size();
    display_bridge_get_block_sizes(&impl->block_size_small, &impl->block_size_large);
    if (impl->block_size_small <= 0) {
        impl->block_size_small = ESP_LV_ADAPTER_BRIDGE_BLOCK_SIZE_SMALL_DEFAULT;
    }
    if (impl->block_size_large <= 0) {
        impl->block_size_large = impl->block_size_small * 8;
    }
    if (impl->block_size_large < impl->block_size_small) {
        impl->block_size_large = impl->block_size_small;
    }

    /* Use common function for runtime info initialization */
    display_bridge_init_runtime_info(&impl->runtime, cfg);

    /* Use common function for frame buffer pointer initialization */
    display_bridge_init_frame_buffer_pointers(
        &impl->front_fb,
        &impl->back_fb,
        &impl->spare_fb,
        &impl->rgb_last_buf,
        &impl->rgb_next_buf,
        &impl->rgb_flush_next_buf,
        &impl->runtime
    );

    display_dirty_region_reset(&impl->dirty);

    if (impl->cfg.base.profile.interface == ESP_LV_ADAPTER_PANEL_IF_OTHER &&
            impl->runtime.rotation != ESP_LV_ADAPTER_ROTATE_0 &&
            impl->cfg.base.profile.mono_layout == ESP_LV_ADAPTER_MONO_LAYOUT_NONE) {
        ESP_LOGW(TAG, "rotation=%d configured on panel interface OTHER; adapter will not apply"
                 " rotation. Configure the LCD panel orientation during panel initialization.",
                 impl->runtime.rotation);
    }

    if (impl->cfg.base.profile.interface == ESP_LV_ADAPTER_PANEL_IF_OTHER && !impl->cfg.base.panel_io) {
        ESP_LOGE(TAG, "panel_io handle required for interface OTHER");
        free(impl);
        return NULL;
    }

    display_bridge_v8_register_vsync(impl);

    return &impl->base;
#endif
}

#if LVGL_VERSION_MAJOR < 9

/**
 * @brief Destroy LVGL v8 display bridge
 *
 * Cleanup function for v8 bridge implementation. Delegates to common destroy
 * function as the cleanup logic is identical across v8 and v9.
 *
 * @param[in] bridge Pointer to the display bridge to destroy
 */
static void display_bridge_v8_destroy(esp_lv_adapter_display_bridge_t *bridge)
{
    /* Use unified destroy implementation for v8/v9 compatibility */
    display_bridge_common_destroy(bridge);
}

/**
 * @brief Update panel handle and configuration for rebind
 */
static esp_err_t display_bridge_v8_update_panel(esp_lv_adapter_display_bridge_t *bridge,
                                                const esp_lv_adapter_display_runtime_config_t *cfg)
{
    ESP_RETURN_ON_FALSE(bridge && cfg, ESP_ERR_INVALID_ARG, TAG, "Invalid arguments");

    esp_lv_adapter_display_bridge_v8_t *impl = (esp_lv_adapter_display_bridge_v8_t *)bridge;

    /* Update panel handle */
    impl->panel = cfg->base.panel;

    /* Update configuration */
    impl->cfg = *cfg;

    /* Reinitialize runtime info with new configuration */
    display_bridge_init_runtime_info(&impl->runtime, cfg);

    /* Reinitialize frame buffer pointers */
    display_bridge_init_frame_buffer_pointers(
        &impl->front_fb,
        &impl->back_fb,
        &impl->spare_fb,
        &impl->rgb_last_buf,
        &impl->rgb_next_buf,
        &impl->rgb_flush_next_buf,
        &impl->runtime
    );

    /* Reset dirty region tracking */
    display_dirty_region_reset(&impl->dirty);

    /* Re-register VSYNC callbacks for new panel */
    if (cfg->base.panel) {
        display_bridge_v8_register_vsync(impl);
    }

    return ESP_OK;
}

/**********************
 *   CORE CALLBACKS
 **********************/

/**
 * @brief Main flush entry point
 */
static void display_bridge_v8_flush_entry(esp_lv_adapter_display_bridge_t *bridge,
                                          void *disp_ref,
                                          const lv_area_t *area,
                                          uint8_t *color_map)
{
    esp_lv_adapter_display_bridge_v8_t *impl = (esp_lv_adapter_display_bridge_v8_t *)bridge;
    lv_disp_drv_t *drv = (lv_disp_drv_t *)disp_ref;
    if (!impl || !area || !drv) {
        if (drv) {
            display_manager_flush_ready(drv);
        }
        return;
    }

    /* Safety check: panel detached for sleep management */
    if (!impl->panel) {
        ESP_LOGD(TAG, "Panel detached, skipping flush");
        display_manager_flush_ready(drv);
        return;
    }

    if (impl->dummy_draw) {
        display_manager_flush_ready(drv);
        return;
    }

    if (!display_bridge_v8_prepare_mono(impl, drv, area, &color_map)) {
        display_manager_flush_ready(drv);
        return;
    }

    const bool mono_rotated = (impl->cfg.base.profile.mono_layout != ESP_LV_ADAPTER_MONO_LAYOUT_NONE &&
                               impl->cfg.base.profile.rotation != ESP_LV_ADAPTER_ROTATE_0);
    lv_area_t phy_area;
    const lv_area_t *flush_area = area;

    if (mono_rotated) {
        uint16_t pw = impl->cfg.base.profile.hor_res;
        uint16_t ph = impl->cfg.base.profile.ver_res;
        int x1, y1, x2, y2;

        switch (impl->cfg.base.profile.rotation) {
        case ESP_LV_ADAPTER_ROTATE_90:
            x1 = pw - 1 - area->y2;
            x2 = pw - 1 - area->y1;
            y1 = area->x1;
            y2 = area->x2;
            break;
        case ESP_LV_ADAPTER_ROTATE_180:
            x1 = pw - 1 - area->x2;
            x2 = pw - 1 - area->x1;
            y1 = ph - 1 - area->y2;
            y2 = ph - 1 - area->y1;
            break;
        case ESP_LV_ADAPTER_ROTATE_270:
            x1 = area->y1;
            x2 = area->y2;
            y1 = ph - 1 - area->x2;
            y2 = ph - 1 - area->x1;
            break;
        default:
            x1 = y1 = x2 = y2 = 0;
            break;
        }

        if (impl->cfg.base.profile.mono_layout == ESP_LV_ADAPTER_MONO_LAYOUT_VTILED) {
            y1 &= ~7;
            y2 |= 7;
        }

        phy_area.x1 = x1;
        phy_area.y1 = y1;
        phy_area.x2 = x2;
        phy_area.y2 = y2;
        flush_area = &phy_area;
    }

    const esp_lv_adapter_rotation_t rotation = bridge_rotation(impl);
    const esp_lv_adapter_tear_avoid_mode_t tear_avoid_mode = impl->cfg.base.tear_avoid_mode;
    const bool need_rotate = (rotation != ESP_LV_ADAPTER_ROTATE_0) &&
                             (impl->cfg.base.profile.interface != ESP_LV_ADAPTER_PANEL_IF_OTHER);

    if (need_rotate) {
        switch (tear_avoid_mode) {
        case ESP_LV_ADAPTER_TEAR_AVOID_MODE_TRIPLE_PARTIAL:
            display_bridge_v8_flush_partial_rotate(impl, drv, flush_area, color_map);
            break;
        case ESP_LV_ADAPTER_TEAR_AVOID_MODE_TRIPLE_FULL:
        case ESP_LV_ADAPTER_TEAR_AVOID_MODE_DOUBLE_FULL:
            display_bridge_v8_flush_full_rotate(impl, drv, flush_area, color_map);
            break;
        case ESP_LV_ADAPTER_TEAR_AVOID_MODE_DOUBLE_DIRECT:
            display_bridge_v8_flush_direct_rotate(impl, drv, flush_area, color_map);
            break;
        default:
            ESP_LOGE(TAG, "Unsupported tear mode: %d", tear_avoid_mode);
            display_manager_flush_ready(drv);
            break;
        }
        return;
    }

    switch (tear_avoid_mode) {
    case ESP_LV_ADAPTER_TEAR_AVOID_MODE_DOUBLE_FULL:
        display_bridge_v8_flush_double_full(impl, drv, flush_area, color_map);
        break;
    case ESP_LV_ADAPTER_TEAR_AVOID_MODE_TRIPLE_FULL:
        display_bridge_v8_flush_triple_full(impl, drv, flush_area, color_map);
        break;
    case ESP_LV_ADAPTER_TEAR_AVOID_MODE_DOUBLE_DIRECT:
        display_bridge_v8_flush_double_direct(impl, drv, flush_area, color_map);
        break;
    case ESP_LV_ADAPTER_TEAR_AVOID_MODE_TRIPLE_PARTIAL:
        display_bridge_v8_flush_triple_diff(impl, drv, flush_area, color_map);
        break;
    case ESP_LV_ADAPTER_TEAR_AVOID_MODE_TE_SYNC:
        display_bridge_v8_flush_gpio_te(impl, drv, flush_area, color_map);
        break;
    default:
        display_bridge_v8_flush_default(impl, drv, flush_area, color_map);
        break;
    }
}

/**
 * @brief Set dummy draw mode
 */
static void display_bridge_v8_set_dummy_draw(esp_lv_adapter_display_bridge_t *bridge, bool enable)
{
    esp_lv_adapter_display_bridge_v8_t *impl = (esp_lv_adapter_display_bridge_v8_t *)bridge;
    if (!impl) {
        return;
    }

    impl->dummy_draw = enable;
    if (!enable) {
        impl->dummy_draw_wait_task = NULL;
        impl->dummy_draw_wait_mask = 0;
    }
}

/**
 * @brief Update dummy draw callbacks
 */
static void display_bridge_v8_set_dummy_draw_callbacks(esp_lv_adapter_display_bridge_t *bridge,
                                                       const esp_lv_adapter_dummy_draw_callbacks_t *cbs,
                                                       void *user_ctx)
{
    esp_lv_adapter_display_bridge_v8_t *impl = (esp_lv_adapter_display_bridge_v8_t *)bridge;
    if (!impl) {
        return;
    }

    if (cbs) {
        impl->cfg.dummy_draw_cbs = *cbs;
        impl->cfg.dummy_draw_user_ctx = user_ctx;
    } else {
        memset(&impl->cfg.dummy_draw_cbs, 0, sizeof(impl->cfg.dummy_draw_cbs));
        impl->cfg.dummy_draw_user_ctx = NULL;
    }
}

static esp_err_t display_bridge_v8_dummy_draw_blit(esp_lv_adapter_display_bridge_t *bridge,
                                                   int x_start,
                                                   int y_start,
                                                   int x_end,
                                                   int y_end,
                                                   const void *frame_buffer,
                                                   bool wait)
{
    esp_lv_adapter_display_bridge_v8_t *impl = (esp_lv_adapter_display_bridge_v8_t *)bridge;
    if (!impl || !frame_buffer || !impl->panel) {
        return ESP_ERR_INVALID_ARG;
    }

    if (x_start < 0 || y_start < 0 || x_end <= x_start || y_end <= y_start) {
        return ESP_ERR_INVALID_ARG;
    }

    uint32_t wait_mask = wait ? ESP_LV_ADAPTER_DUMMY_DRAW_EVT_COLOR_DONE : 0;

    if (!impl->dummy_draw) {
        return ESP_ERR_INVALID_STATE;
    }

    if (wait_mask != 0) {
        if (impl->dummy_draw_wait_task) {
            return ESP_ERR_INVALID_STATE;
        }
        impl->dummy_draw_wait_task = xTaskGetCurrentTaskHandle();
        impl->dummy_draw_wait_mask = wait_mask;
    } else {
        impl->dummy_draw_wait_task = NULL;
        impl->dummy_draw_wait_mask = 0;
    }

    esp_err_t ret = display_lcd_blit_area(impl->panel, x_start, y_start, x_end, y_end, frame_buffer);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Blit area failed: %s", esp_err_to_name(ret));
        impl->dummy_draw_wait_task = NULL;
        impl->dummy_draw_wait_mask = 0;
        return ret;
    }

    if (wait_mask == 0) {
        return ESP_OK;
    }

    uint32_t pending = wait_mask;
    while (pending != 0) {
        uint32_t events = 0;
        (void)xTaskNotifyWait(0, wait_mask, &events, portMAX_DELAY);
        pending &= ~events;
    }

    impl->dummy_draw_wait_task = NULL;
    impl->dummy_draw_wait_mask = 0;
    return ESP_OK;
}

/**
 * @brief Wrapper callback for area rounding (LVGL v8)
 */
static void rounder_cb_wrapper(lv_disp_drv_t *drv, lv_area_t *area)
{
    /* user_data points to display_node, get bridge from node */
    esp_lv_adapter_display_node_t *node = (esp_lv_adapter_display_node_t *)drv->user_data;
    if (!node || !node->bridge) {
        return;
    }

    esp_lv_adapter_display_bridge_v8_t *impl = (esp_lv_adapter_display_bridge_v8_t *)node->bridge;
    if (impl->cfg.rounder_cb) {
        impl->cfg.rounder_cb(area, impl->cfg.rounder_user_data);
    }
}

/**
 * @brief Set area rounding callback
 */
static void display_bridge_v8_set_area_rounder(esp_lv_adapter_display_bridge_t *bridge,
                                               void (*rounder_cb)(lv_area_t *, void *),
                                               void *user_data)
{
    esp_lv_adapter_display_bridge_v8_t *impl = (esp_lv_adapter_display_bridge_v8_t *)bridge;
    if (!impl) {
        return;
    }

    impl->cfg.rounder_cb = rounder_cb;
    impl->cfg.rounder_user_data = user_data;

    lv_disp_drv_t *drv = impl->cfg.lv_disp ? impl->cfg.lv_disp->driver : NULL;
    if (drv) {
        drv->rounder_cb = rounder_cb ? rounder_cb_wrapper : NULL;
    }
}

/**********************
 *   DUMMY DRAW EVENTS
 **********************/

static inline void display_bridge_v8_signal_dummy_draw_event(esp_lv_adapter_display_bridge_v8_t *impl,
                                                             uint32_t event_bit,
                                                             BaseType_t *need_yield)
{
    if (!impl) {
        return;
    }
    if (!(impl->dummy_draw_wait_mask & event_bit)) {
        return;
    }
    TaskHandle_t task = impl->dummy_draw_wait_task;
    if (!task) {
        return;
    }
    xTaskNotifyFromISR(task, event_bit, eSetBits, need_yield);
}

/**********************
 *   VSYNC HANDLING
 **********************/

/**
 * @brief VSync event handler (common logic)
 */
static bool IRAM_ATTR display_bridge_v8_handle_vsync(esp_lv_adapter_display_bridge_v8_t *impl)
{
    if (!impl) {
        return false;
    }
    BaseType_t need_yield = pdFALSE;

    if (impl->dummy_draw && impl->cfg.dummy_draw_cbs.on_vsync) {
        lv_disp_t *disp = impl->cfg.lv_disp ? impl->cfg.lv_disp : lv_disp_get_default();
        impl->cfg.dummy_draw_cbs.on_vsync((lv_display_t *)disp, true, impl->cfg.dummy_draw_user_ctx);
    }

    if (impl->cfg.base.tear_avoid_mode == ESP_LV_ADAPTER_TEAR_AVOID_MODE_TRIPLE_FULL &&
            bridge_rotation(impl) == ESP_LV_ADAPTER_ROTATE_0 && impl->rgb_next_buf != impl->rgb_last_buf) {
        impl->rgb_flush_next_buf = impl->rgb_last_buf;
        impl->rgb_last_buf = impl->rgb_next_buf;
    }

    if (impl->cfg.base.tear_avoid_mode == ESP_LV_ADAPTER_TEAR_AVOID_MODE_NONE) {
        lv_disp_t *disp = impl->cfg.lv_disp ? impl->cfg.lv_disp : lv_disp_get_default();
        if (disp) {
            display_manager_flush_ready(disp->driver);
        }
        return (need_yield == pdTRUE);
    } else if (impl->cfg.base.tear_avoid_mode == ESP_LV_ADAPTER_TEAR_AVOID_MODE_TE_SYNC ||
               impl->cfg.base.tear_avoid_mode == ESP_LV_ADAPTER_TEAR_AVOID_MODE_DOUBLE_FULL ||
               impl->cfg.base.tear_avoid_mode == ESP_LV_ADAPTER_TEAR_AVOID_MODE_DOUBLE_DIRECT) {
        TaskHandle_t notify_task = impl->notify_task;
        if (!notify_task) {
            esp_lv_adapter_context_t *ctx = esp_lv_adapter_get_context();
            if (ctx) {
                notify_task = ctx->task;
            }
        }

        if (notify_task) {
            vTaskNotifyGiveFromISR(notify_task, &need_yield);
        }
    }

    return (need_yield == pdTRUE);
}

#if CONFIG_SOC_MIPI_DSI_SUPPORTED
static bool IRAM_ATTR display_bridge_v8_on_mipi_color_trans_done(esp_lcd_panel_handle_t panel,
                                                                 esp_lcd_dpi_panel_event_data_t *event_data,
                                                                 void *user_ctx)
{
    (void)panel;
    (void)event_data;
    esp_lv_adapter_display_bridge_v8_t *impl = (esp_lv_adapter_display_bridge_v8_t *)user_ctx;

    /* Safety check: panel detached for sleep management */
    if (!impl || !impl->panel) {
        return false;
    }

    BaseType_t need_yield = pdFALSE;
    display_bridge_v8_signal_dummy_draw_event(impl, ESP_LV_ADAPTER_DUMMY_DRAW_EVT_COLOR_DONE, &need_yield);
    if (impl->dummy_draw && impl->cfg.dummy_draw_cbs.on_color_trans_done) {
        lv_disp_t *disp = impl->cfg.lv_disp ? impl->cfg.lv_disp : lv_disp_get_default();
        impl->cfg.dummy_draw_cbs.on_color_trans_done((lv_display_t *)disp, true, impl->cfg.dummy_draw_user_ctx);
    }
    bool vsync = false;
    if (impl->cfg.base.tear_avoid_mode == ESP_LV_ADAPTER_TEAR_AVOID_MODE_NONE) {
        vsync = display_bridge_v8_handle_vsync(impl);
    }
    return vsync || (need_yield == pdTRUE);
}

static bool IRAM_ATTR display_bridge_v8_on_mipi_refresh_done(esp_lcd_panel_handle_t panel,
                                                             esp_lcd_dpi_panel_event_data_t *event_data,
                                                             void *user_ctx)
{
    (void)panel;
    (void)event_data;
    esp_lv_adapter_display_bridge_v8_t *impl = (esp_lv_adapter_display_bridge_v8_t *)user_ctx;

    /* Safety check: panel detached for sleep management */
    if (!impl || !impl->panel) {
        return false;
    }

    BaseType_t need_yield = pdFALSE;
    display_bridge_v8_signal_dummy_draw_event(impl, ESP_LV_ADAPTER_DUMMY_DRAW_EVT_FRAME_DONE, &need_yield);
    bool vsync = display_bridge_v8_handle_vsync(impl);
    return vsync || (need_yield == pdTRUE);
}
#endif

#if CONFIG_SOC_LCD_RGB_SUPPORTED
static bool IRAM_ATTR display_bridge_v8_on_rgb_color_trans_done(esp_lcd_panel_handle_t panel,
                                                                const esp_lcd_rgb_panel_event_data_t *event_data,
                                                                void *user_ctx)
{
    (void)panel;
    (void)event_data;
    esp_lv_adapter_display_bridge_v8_t *impl = (esp_lv_adapter_display_bridge_v8_t *)user_ctx;

    /* Safety check: panel detached for sleep management */
    if (!impl || !impl->panel) {
        return false;
    }

    BaseType_t need_yield = pdFALSE;
    display_bridge_v8_signal_dummy_draw_event(impl, ESP_LV_ADAPTER_DUMMY_DRAW_EVT_COLOR_DONE, &need_yield);
    if (impl->dummy_draw && impl->cfg.dummy_draw_cbs.on_color_trans_done) {
        lv_disp_t *disp = impl->cfg.lv_disp ? impl->cfg.lv_disp : lv_disp_get_default();
        impl->cfg.dummy_draw_cbs.on_color_trans_done((lv_display_t *)disp, true, impl->cfg.dummy_draw_user_ctx);
    }
    bool vsync = false;
    if (impl->cfg.base.tear_avoid_mode == ESP_LV_ADAPTER_TEAR_AVOID_MODE_NONE) {
        vsync = display_bridge_v8_handle_vsync(impl);
    }
    return vsync || (need_yield == pdTRUE);
}

static bool IRAM_ATTR display_bridge_v8_on_rgb_frame_complete(esp_lcd_panel_handle_t panel,
                                                              const esp_lcd_rgb_panel_event_data_t *event_data,
                                                              void *user_ctx)
{
    (void)panel;
    (void)event_data;
    esp_lv_adapter_display_bridge_v8_t *impl = (esp_lv_adapter_display_bridge_v8_t *)user_ctx;

    /* Safety check: panel detached for sleep management */
    if (!impl || !impl->panel) {
        return false;
    }

    BaseType_t need_yield = pdFALSE;
    display_bridge_v8_signal_dummy_draw_event(impl, ESP_LV_ADAPTER_DUMMY_DRAW_EVT_FRAME_DONE, &need_yield);
    bool vsync = display_bridge_v8_handle_vsync(impl);
    return vsync || (need_yield == pdTRUE);
}
#endif

static bool IRAM_ATTR display_bridge_v8_on_io_color_trans_done(esp_lcd_panel_io_handle_t panel_io,
                                                               esp_lcd_panel_io_event_data_t *edata,
                                                               void *user_ctx)
{
    (void)panel_io;
    (void)edata;
    esp_lv_adapter_display_bridge_v8_t *impl = (esp_lv_adapter_display_bridge_v8_t *)user_ctx;

    /* Safety check: panel detached for sleep management */
    if (!impl || !impl->panel) {
        return false;
    }

    BaseType_t need_yield = pdFALSE;
    display_bridge_v8_signal_dummy_draw_event(impl, ESP_LV_ADAPTER_DUMMY_DRAW_EVT_COLOR_DONE, &need_yield);
    if (impl->cfg.te_ctx) {
        esp_lv_adapter_te_sync_record_tx_done(impl->cfg.te_ctx);
    }
    if (impl->dummy_draw && impl->cfg.dummy_draw_cbs.on_color_trans_done) {
        lv_disp_t *disp = impl->cfg.lv_disp ? impl->cfg.lv_disp : lv_disp_get_default();
        impl->cfg.dummy_draw_cbs.on_color_trans_done((lv_display_t *)disp, true, impl->cfg.dummy_draw_user_ctx);
    }
    bool vsync = display_bridge_v8_handle_vsync(impl);
    return vsync || (need_yield == pdTRUE);
}

/**
 * @brief Register VSync callbacks based on panel interface type
 */
static void display_bridge_v8_register_vsync(esp_lv_adapter_display_bridge_v8_t *impl)
{
    if (!impl || !impl->panel) {
        return;
    }

    switch (impl->cfg.base.profile.interface) {
    case ESP_LV_ADAPTER_PANEL_IF_MIPI_DSI:
#if CONFIG_SOC_MIPI_DSI_SUPPORTED
    {
        esp_lcd_dpi_panel_event_callbacks_t cbs = {
            .on_color_trans_done = display_bridge_v8_on_mipi_color_trans_done,
            .on_refresh_done = display_bridge_v8_on_mipi_refresh_done,
        };

        esp_err_t ret = esp_lcd_dpi_panel_register_event_callbacks(impl->panel, &cbs, impl);
        if (ret != ESP_OK) {
            ESP_LOGW(TAG, "register panel callbacks failed (%d)", ret);
        }
        break;
    }
#else
    break;
#endif
    case ESP_LV_ADAPTER_PANEL_IF_RGB:
#if CONFIG_SOC_LCD_RGB_SUPPORTED
    {
        esp_lcd_rgb_panel_event_callbacks_t cbs = {
            .on_color_trans_done = display_bridge_v8_on_rgb_color_trans_done,
            .on_frame_buf_complete = display_bridge_v8_on_rgb_frame_complete,
        };

        esp_err_t ret = esp_lcd_rgb_panel_register_event_callbacks(impl->panel, &cbs, impl);
        if (ret != ESP_OK) {
            ESP_LOGW(TAG, "register panel callbacks failed (%d)", ret);
        }
        break;
    }
#else
        /* no RGB support, fallthrough to IO registration */
#endif
    default: {
        esp_lcd_panel_io_handle_t panel_io = impl->cfg.base.panel_io;
        if (!panel_io) {
            ESP_LOGW(TAG, "panel_io handle missing, skip IO callbacks");
            break;
        }

        esp_lcd_panel_io_callbacks_t cbs = {
            .on_color_trans_done = display_bridge_v8_on_io_color_trans_done,
        };
        esp_err_t ret = esp_lcd_panel_io_register_event_callbacks(panel_io, &cbs, impl);
        if (ret != ESP_OK) {
            ESP_LOGW(TAG, "register panel IO callbacks failed (%d)", ret);
        }
        break;
    }
    }
}

/**********************
 *   FLUSH IMPLEMENTATIONS
 **********************/

/**
 * @brief Default flush (single buffer, no tear protection)
 */
static void display_bridge_v8_flush_default(esp_lv_adapter_display_bridge_v8_t *impl,
                                            lv_disp_drv_t *drv,
                                            const lv_area_t *area,
                                            uint8_t *color_map)
{
    esp_lcd_panel_handle_t panel_handle = impl->panel;
    const int offsetx1 = area->x1;
    const int offsetx2 = area->x2;
    const int offsety1 = area->y1;
    const int offsety2 = area->y2;

    esp_err_t ret = esp_lcd_panel_draw_bitmap(panel_handle, offsetx1, offsety1, offsetx2 + 1, offsety2 + 1, color_map);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Draw bitmap failed: %s", esp_err_to_name(ret));
        /* Callback won't be triggered on failure, must notify LVGL immediately */
        display_manager_flush_ready(drv);
    }
}

/**
 * @brief GPIO TE synchronized flush
 */
static void display_bridge_v8_flush_gpio_te(esp_lv_adapter_display_bridge_v8_t *impl,
                                            lv_disp_drv_t *drv,
                                            const lv_area_t *area,
                                            uint8_t *color_map)
{
    esp_lcd_panel_handle_t panel_handle = impl->panel;
    const int offsetx1 = area->x1;
    const int offsetx2 = area->x2;
    const int offsety1 = area->y1;
    const int offsety2 = area->y2;

    /* Notify TE sync to start a new frame */
    if (impl->cfg.te_ctx) {
        esp_lv_adapter_te_sync_begin_frame(impl->cfg.te_ctx);
    }

    /* Prepare pixel data before waiting for TE to avoid window jitter */
    size_t flush_size = (size_t)lv_area_get_size(area) * bridge_color_bytes(impl);
#if CONFIG_SOC_PPA_SUPPORTED || CONFIG_SOC_DMA2D_SUPPORTED
    display_cache_msync_range(color_map, flush_size, hw_resource.data_cache_line_size);
#else
    display_cache_msync_range(color_map, flush_size, impl->cache_line_size);
#endif

    /* Wait for TE signal to avoid tearing */
    if (impl->cfg.te_ctx) {
        esp_lv_adapter_te_sync_wait_for_vsync(impl->cfg.te_ctx);
    }

    if (impl->cfg.te_ctx) {
        esp_lv_adapter_te_sync_record_tx_start(impl->cfg.te_ctx);
    }

    esp_err_t ret = esp_lcd_panel_draw_bitmap(panel_handle, offsetx1, offsety1, offsetx2 + 1, offsety2 + 1, color_map);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Draw bitmap failed: %s", esp_err_to_name(ret));
        /* Don't wait for callback that will never come */
        display_manager_flush_ready(drv);
        return;
    }

    /* Wait for transmission to complete */
    ulTaskNotifyValueClear(NULL, ULONG_MAX);
    ulTaskNotifyTake(pdTRUE, portMAX_DELAY);

    display_manager_flush_ready(drv);
}

/**
 * @brief Double buffering with full-screen refresh
 */
static void display_bridge_v8_flush_double_full(esp_lv_adapter_display_bridge_v8_t *impl,
                                                lv_disp_drv_t *drv,
                                                const lv_area_t *area,
                                                uint8_t *color_map)
{
    (void)area;
    esp_lcd_panel_handle_t panel_handle = impl->panel;

    esp_err_t ret = display_lcd_blit_full(panel_handle, &impl->runtime, color_map);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Blit failed: %s", esp_err_to_name(ret));
    }

    ulTaskNotifyValueClear(NULL, ULONG_MAX);
    ulTaskNotifyTake(pdTRUE, portMAX_DELAY);

    display_manager_flush_ready(drv);
}

/**
 * @brief Triple buffering with full-screen refresh
 */
static void display_bridge_v8_flush_triple_full(esp_lv_adapter_display_bridge_v8_t *impl,
                                                lv_disp_drv_t *drv,
                                                const lv_area_t *area,
                                                uint8_t *color_map)
{
    (void)area;
    esp_lcd_panel_handle_t panel_handle = impl->panel;

    lv_disp_draw_buf_t *draw_buf = drv->draw_buf;
    if (draw_buf && draw_buf->buf2) {
        if (draw_buf->buf_act == draw_buf->buf1) {
            draw_buf->buf2 = (lv_color_t *)impl->rgb_flush_next_buf;
        } else {
            draw_buf->buf1 = (lv_color_t *)impl->rgb_flush_next_buf;
        }
    }
    impl->rgb_flush_next_buf = color_map;

    esp_err_t ret = display_lcd_blit_full(panel_handle, &impl->runtime, color_map);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Blit failed: %s", esp_err_to_name(ret));
    }

    impl->rgb_next_buf = color_map;

    display_manager_flush_ready(drv);
}

/**
 * @brief Double buffering with direct mode
 */
static void display_bridge_v8_flush_double_direct(esp_lv_adapter_display_bridge_v8_t *impl,
                                                  lv_disp_drv_t *drv,
                                                  const lv_area_t *area,
                                                  uint8_t *color_map)
{
    (void)area;
    esp_lcd_panel_handle_t panel_handle = impl->panel;

    if (lv_disp_flush_is_last(drv)) {
        esp_err_t ret = display_lcd_blit_full(panel_handle, &impl->runtime, color_map);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "Blit failed: %s", esp_err_to_name(ret));
        }

        ulTaskNotifyValueClear(NULL, ULONG_MAX);
        ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
    }

    display_manager_flush_ready(drv);
}

/**
 * @brief Triple buffering with partial differential update
 */
static void display_bridge_v8_flush_triple_diff(esp_lv_adapter_display_bridge_v8_t *impl,
                                                lv_disp_drv_t *drv,
                                                const lv_area_t *area,
                                                uint8_t *color_map)
{
    esp_lcd_panel_handle_t panel = impl->panel;
    uint8_t lvgl_color_format_bytes = bridge_color_bytes(impl);
    uint16_t lvgl_port_h_res = bridge_h_res(impl);
#if SOC_DMA2D_SUPPORTED
    uint16_t lvgl_port_v_res = bridge_v_res(impl);
#endif

#if SOC_DMA2D_SUPPORTED
    size_t rect_w = area->x2 - area->x1 + 1;
    size_t rect_h = area->y2 - area->y1 + 1;

    uint8_t *row0 = (uint8_t *)color_map;
    uint8_t *row1 = (uint8_t *)color_map + rect_w * lvgl_color_format_bytes;
    size_t stride_bytes = row1 - row0;
    size_t stride_px = stride_bytes / lvgl_color_format_bytes;

    bool use_full_stride = (stride_px == lvgl_port_h_res);
    size_t src_stride_px = use_full_stride ? lvgl_port_h_res : rect_w;
    size_t src_offset_x = use_full_stride ? area->x1 : 0;

    size_t flush_size = src_stride_px * rect_h * lvgl_color_format_bytes;
    display_cache_msync_range(color_map, flush_size, hw_resource.data_cache_line_size);

    esp_async_fbcpy_trans_desc_t blit = {
        .src_buffer        = color_map,
        .dst_buffer        = impl->back_fb,
        .src_buffer_size_x = src_stride_px,
        .src_buffer_size_y = rect_h,
        .src_offset_x      = src_offset_x,
        .src_offset_y      = 0,
        .dst_buffer_size_x = lvgl_port_h_res,
        .dst_buffer_size_y = lvgl_port_v_res,
        .dst_offset_x      = area->x1,
        .dst_offset_y      = area->y1,
        .copy_size_x       = rect_w,
        .copy_size_y       = rect_h,
        .pixel_format_unique_id = {
            .color_type_id = (lvgl_color_format_bytes == COLOR_BYTES_RGB565) ?
            COLOR_TYPE_ID(COLOR_SPACE_RGB, COLOR_PIXEL_RGB565) :
            COLOR_TYPE_ID(COLOR_SPACE_RGB, COLOR_PIXEL_RGB888)
        },
    };

    ESP_ERROR_CHECK(display_bridge_dma2d_copy_sync(&blit, portMAX_DELAY));

#else
    const int bytes_per_pixel = lvgl_color_format_bytes;
    const int bytes_per_line = lvgl_port_h_res * bytes_per_pixel;
    const int copy_bytes = (area->x2 - area->x1 + 1) * bytes_per_pixel;
    uint8_t *src = (uint8_t *)color_map;
    uint8_t *dst = (uint8_t *)impl->back_fb + (area->y1 * lvgl_port_h_res + area->x1) * bytes_per_pixel;

    int num_rows = area->y2 - area->y1 + 1;
    bool is_full_width = (copy_bytes == bytes_per_line);

    if (is_full_width && num_rows > 4) {
        size_t total_bytes = (size_t)num_rows * bytes_per_line;
        memcpy(dst, src, total_bytes);
    } else if (num_rows > 16 && copy_bytes > 64) {
        const int batch_rows = 8;
        int full_batches = num_rows / batch_rows;
        int remaining_rows = num_rows % batch_rows;

        for (int batch = 0; batch < full_batches; batch++) {
            for (int r = 0; r < batch_rows; r++) {
                memcpy(dst, src, copy_bytes);
                dst += bytes_per_line;
                src += copy_bytes;
            }
        }

        for (int r = 0; r < remaining_rows; r++) {
            memcpy(dst, src, copy_bytes);
            dst += bytes_per_line;
            src += copy_bytes;
        }
    } else if (copy_bytes == 2 && num_rows <= 8) {
        uint16_t *dst16 = (uint16_t *)dst;
        uint16_t *src16 = (uint16_t *)src;
        for (int y = 0; y < num_rows; y++) {
            *dst16 = *src16;
            dst16 += lvgl_port_h_res;
            src16++;
        }
    } else {
        for (int y = area->y1; y <= area->y2; y++) {
            memcpy(dst, src, copy_bytes);
            dst += bytes_per_line;
            src += copy_bytes;
        }
    }

#endif

    if (lv_disp_flush_is_last(drv)) {
        lv_disp_t *disp_refr = _lv_refr_get_disp_refreshing();

        copy_unrendered_area_from_front_to_back(disp_refr, impl);

        esp_err_t ret = display_lcd_blit_full(panel, &impl->runtime, impl->back_fb);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "Blit failed: %s", esp_err_to_name(ret));
        } else {
            void *tmp = impl->front_fb;
            impl->front_fb = impl->back_fb;
            impl->back_fb  = impl->spare_fb;
            impl->spare_fb = tmp;
        }
    }

    display_manager_flush_ready(drv);
}

/**
 * @brief Direct mode with rotation support
 */
static void display_bridge_v8_flush_direct_rotate(esp_lv_adapter_display_bridge_v8_t *impl,
                                                  lv_disp_drv_t *drv,
                                                  const lv_area_t *area,
                                                  uint8_t *color_map)
{
    esp_lcd_panel_handle_t panel_handle = impl->panel;
    const int offsetx1 = area->x1;
    const int offsetx2 = area->x2;
    const int offsety1 = area->y1;
    const int offsety2 = area->y2;
    void *next_fb = NULL;
    esp_lv_adapter_display_flush_probe_t probe_result = ESP_LV_ADAPTER_DISPLAY_FLUSH_PROBE_PART_COPY;
    lv_disp_t *disp = _lv_refr_get_disp_refreshing();
    esp_err_t ret;

    if (lv_disp_flush_is_last(drv)) {
        if (drv->full_refresh) {
            drv->full_refresh = 0;

            uint8_t color_bytes = bridge_color_bytes(impl);

            next_fb = display_runtime_acquire_next_buffer(&impl->runtime, &impl->toggle_fb);
            rotate_copy_region(impl, color_map, next_fb,
                               offsetx1, offsety1, offsetx2, offsety2,
                               LV_HOR_RES, LV_VER_RES, bridge_rotation(impl), color_bytes);

            ret = display_lcd_blit_full(panel_handle, &impl->runtime, next_fb);
            if (ret != ESP_OK) {
                ESP_LOGE(TAG, "Blit failed: %s", esp_err_to_name(ret));
            }

            ulTaskNotifyValueClear(NULL, ULONG_MAX);
            ulTaskNotifyTake(pdTRUE, portMAX_DELAY);

            void *sync_fb = display_runtime_acquire_next_buffer(&impl->runtime, &impl->toggle_fb);
            flush_dirty_copy(impl, sync_fb, color_map, &impl->dirty);
            display_runtime_acquire_next_buffer(&impl->runtime, &impl->toggle_fb);
        } else {
            probe_result = flush_copy_probe(drv, disp);

            if (probe_result == ESP_LV_ADAPTER_DISPLAY_FLUSH_PROBE_FULL_COPY) {
                flush_dirty_save(&impl->dirty);

                drv->full_refresh = 1;
                disp->rendering_in_progress = false;
                display_manager_flush_ready(drv);

                lv_refr_now(_lv_refr_get_disp_refreshing());
                return;
            } else {
                next_fb = display_runtime_acquire_next_buffer(&impl->runtime, &impl->toggle_fb);
                flush_dirty_save(&impl->dirty);
                flush_dirty_copy(impl, next_fb, color_map, &impl->dirty);

                ret = display_lcd_blit_full(panel_handle, &impl->runtime, next_fb);
                if (ret != ESP_OK) {
                    ESP_LOGE(TAG, "Blit failed: %s", esp_err_to_name(ret));
                }

                ulTaskNotifyValueClear(NULL, ULONG_MAX);
                ulTaskNotifyTake(pdTRUE, portMAX_DELAY);

                if (probe_result == ESP_LV_ADAPTER_DISPLAY_FLUSH_PROBE_PART_COPY) {
                    flush_dirty_save(&impl->dirty);
                    void *sync_fb = display_runtime_acquire_next_buffer(&impl->runtime, &impl->toggle_fb);
                    flush_dirty_copy(impl, sync_fb, color_map, &impl->dirty);
                    display_runtime_acquire_next_buffer(&impl->runtime, &impl->toggle_fb);
                }
            }
        }
    }

    display_manager_flush_ready(drv);
}

/**
 * @brief Full refresh with rotation
 */
static void display_bridge_v8_flush_full_rotate(esp_lv_adapter_display_bridge_v8_t *impl,
                                                lv_disp_drv_t *drv,
                                                const lv_area_t *area,
                                                uint8_t *color_map)
{
    esp_lcd_panel_handle_t panel_handle = impl->panel;

    const int offsetx1 = area->x1;
    const int offsetx2 = area->x2;
    const int offsety1 = area->y1;
    const int offsety2 = area->y2;
    void *next_fb = display_runtime_acquire_next_buffer(&impl->runtime, &impl->toggle_fb);
    uint8_t color_bytes = bridge_color_bytes(impl);

    rotate_copy_region(impl, color_map, next_fb,
                       offsetx1, offsety1, offsetx2, offsety2,
                       LV_HOR_RES, LV_VER_RES, bridge_rotation(impl), color_bytes);

    esp_err_t ret = display_lcd_blit_full(panel_handle, &impl->runtime, next_fb);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Blit failed: %s", esp_err_to_name(ret));
    }

    display_manager_flush_ready(drv);
}

/**
 * @brief Partial refresh with rotation
 */
static void display_bridge_v8_flush_partial_rotate(esp_lv_adapter_display_bridge_v8_t *impl,
                                                   lv_disp_drv_t *drv,
                                                   const lv_area_t *area,
                                                   uint8_t *color_map)
{
    esp_lcd_panel_handle_t panel = impl->panel;
    uint8_t lvgl_color_format_bytes = bridge_color_bytes(impl);
    uint16_t lvgl_port_h_res = bridge_h_res(impl);

    size_t rect_w = area->x2 - area->x1 + 1;

    uint8_t *row0 = (uint8_t *)color_map;
    uint8_t *row1 = row0 + rect_w * lvgl_color_format_bytes;
    size_t stride_bytes = row1 - row0;
    size_t src_stride_px = stride_bytes / lvgl_color_format_bytes;

    bool use_full_stride = (src_stride_px == lvgl_port_h_res);
    if (use_full_stride) {
        src_stride_px = lvgl_port_h_res;
    } else {
        src_stride_px = rect_w;
    }

    rotate_copy_strided_region(color_map,
                               impl->back_fb,
                               area->x1, area->y1,
                               area->x2, area->y2,
                               src_stride_px,
                               impl);

    if (lv_disp_flush_is_last(drv)) {

        display_cache_msync_framebuffer(impl->back_fb, impl->runtime.frame_buffer_size);

        lv_disp_t *disp_refr = _lv_refr_get_disp_refreshing();

        copy_unrendered_area_from_front_to_back(disp_refr, impl);

        esp_err_t ret = display_lcd_blit_full(panel, &impl->runtime, impl->back_fb);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "Blit failed: %s", esp_err_to_name(ret));
        } else {
            void *tmp = impl->front_fb;
            impl->front_fb = impl->back_fb;
            impl->back_fb  = impl->spare_fb;
            impl->spare_fb = tmp;
        }
    }

    display_manager_flush_ready(drv);
}

/**********************
 *   ROTATION & COPY OPERATIONS
 **********************/

/**
 * @brief Rotate and copy region with stride support (uses PPA if available)
 */
static void IRAM_ATTR rotate_copy_strided_region(const void *src, void *dst_fb,
                                                 uint16_t lv_x_start, uint16_t lv_y_start,
                                                 uint16_t lv_x_end,   uint16_t lv_y_end,
                                                 uint16_t src_stride_px,
                                                 esp_lv_adapter_display_bridge_v8_t *impl)
{
    esp_lv_adapter_rotation_t rotation = bridge_rotation(impl);
    uint16_t hor_res = bridge_h_res(impl);
    uint16_t ver_res = bridge_v_res(impl);
    uint8_t color_bytes = bridge_color_bytes(impl);

#if CONFIG_SOC_PPA_SUPPORTED
    if (hw_resource.ppa_handle && (color_bytes == COLOR_BYTES_RGB565 || color_bytes == COLOR_BYTES_RGB888)) {
        const size_t rect_w = lv_x_end - lv_x_start + 1;
        const size_t rect_h = lv_y_end - lv_y_start + 1;

        int block_w_final    = rect_w;
        int block_h_final    = rect_h;
        int block_off_x_final = 0;
        int block_off_y_final = 0;

        ppa_srm_rotation_angle_t ppa_rotation;
        int x_offset = 0, y_offset = 0;

        switch (rotation) {
        case ESP_LV_ADAPTER_ROTATE_90:
            ppa_rotation = PPA_SRM_ROTATION_ANGLE_270;
            x_offset = hor_res - 1 - lv_y_end;
            y_offset = lv_x_start;
            break;
        case ESP_LV_ADAPTER_ROTATE_180:
            ppa_rotation = PPA_SRM_ROTATION_ANGLE_180;
            x_offset = hor_res - 1 - lv_x_end;
            y_offset = ver_res - 1 - lv_y_end;
            break;
        case ESP_LV_ADAPTER_ROTATE_270:
            ppa_rotation = PPA_SRM_ROTATION_ANGLE_90;
            x_offset = lv_y_start;
            y_offset = ver_res - 1 - lv_x_end;
            break;
        default:
            ppa_rotation = PPA_SRM_ROTATION_ANGLE_0;
            break;
        }

        size_t buffer_size = heap_caps_get_allocated_size(dst_fb);
        if (buffer_size == 0) {
            buffer_size = LVGL_PORT_PPA_ALIGN_UP((size_t)color_bytes * hor_res * ver_res, hw_resource.data_cache_line_size);
        }
        ppa_srm_oper_config_t oper_config = {
            .in.buffer          = src,
            .in.pic_w           = src_stride_px,
            .in.pic_h           = rect_h,
            .in.block_w         = block_w_final,
            .in.block_h         = block_h_final,
            .in.block_offset_x  = block_off_x_final,
            .in.block_offset_y  = block_off_y_final,
            .in.srm_cm          = (LV_COLOR_DEPTH == COLOR_DEPTH_RGB888) ? PPA_SRM_COLOR_MODE_RGB888 : PPA_SRM_COLOR_MODE_RGB565,

               .out.buffer         = dst_fb,
               .out.buffer_size    = buffer_size,
               .out.pic_w          = hor_res,
               .out.pic_h          = ver_res,
               .out.block_offset_x = x_offset,
               .out.block_offset_y = y_offset,
               .out.srm_cm         = (LV_COLOR_DEPTH == COLOR_DEPTH_RGB888) ? PPA_SRM_COLOR_MODE_RGB888 : PPA_SRM_COLOR_MODE_RGB565,

               .rotation_angle     = ppa_rotation,
               .scale_x            = PPA_SCALE_FACTOR_NO_SCALE,
               .scale_y            = PPA_SCALE_FACTOR_NO_SCALE,
               .rgb_swap           = PPA_SWAP_DISABLED,
               .byte_swap          = 0,
               .mode               = PPA_TRANS_MODE_BLOCKING,
        };
        ESP_ERROR_CHECK(ppa_do_scale_rotate_mirror(hw_resource.ppa_handle, &oper_config));
        return;
    }
#endif

    display_rotate_copy_region(src,
                               dst_fb,
                               lv_x_start,
                               lv_y_start,
                               lv_x_end,
                               lv_y_end,
                               src_stride_px,
                               hor_res,
                               ver_res,
                               rotation,
                               color_bytes,
                               impl->block_size_small,
                               impl->block_size_large);
}

/**
 * @brief Rotate and copy full region (uses PPA if available)
 */
static void IRAM_ATTR rotate_copy_region(esp_lv_adapter_display_bridge_v8_t *impl,
                                         const void *from, void *to,
                                         uint16_t x_start, uint16_t y_start,
                                         uint16_t x_end, uint16_t y_end,
                                         uint16_t w, uint16_t h,
                                         esp_lv_adapter_rotation_t rotation,
                                         uint8_t color_bytes)
{
#if CONFIG_SOC_PPA_SUPPORTED
    if (hw_resource.ppa_handle && (color_bytes == COLOR_BYTES_RGB565 || color_bytes == COLOR_BYTES_RGB888)) {
        ppa_srm_rotation_angle_t ppa_rotation;
        int x_offset = 0, y_offset = 0;

        switch (rotation) {
        case ESP_LV_ADAPTER_ROTATE_90:
            ppa_rotation = PPA_SRM_ROTATION_ANGLE_270;
            x_offset = h - y_end - 1;
            y_offset = x_start;
            break;
        case ESP_LV_ADAPTER_ROTATE_180:
            ppa_rotation = PPA_SRM_ROTATION_ANGLE_180;
            x_offset = w - x_end - 1;
            y_offset = h - y_end - 1;
            break;
        case ESP_LV_ADAPTER_ROTATE_270:
            ppa_rotation = PPA_SRM_ROTATION_ANGLE_90;
            x_offset = y_start;
            y_offset = w - x_end - 1;
            break;
        default:
            ppa_rotation = PPA_SRM_ROTATION_ANGLE_0;
            break;
        }

        size_t buffer_size = heap_caps_get_allocated_size(to);
        if (buffer_size == 0) {
            buffer_size = LVGL_PORT_PPA_ALIGN_UP((size_t)color_bytes * w * h, hw_resource.data_cache_line_size);
        }
        ppa_srm_oper_config_t oper_config = {
            .in.buffer = from,
            .in.pic_w = w,
            .in.pic_h = h,
            .in.block_w = x_end - x_start + 1,
            .in.block_h = y_end - y_start + 1,
            .in.block_offset_x = x_start,
            .in.block_offset_y = y_start,
            .in.srm_cm = (LV_COLOR_DEPTH == 24) ? PPA_SRM_COLOR_MODE_RGB888 : PPA_SRM_COLOR_MODE_RGB565,

               .out.buffer = to,
               .out.buffer_size = buffer_size,
               .out.pic_w = (ppa_rotation == PPA_SRM_ROTATION_ANGLE_90 || ppa_rotation == PPA_SRM_ROTATION_ANGLE_270) ? h : w,
               .out.pic_h = (ppa_rotation == PPA_SRM_ROTATION_ANGLE_90 || ppa_rotation == PPA_SRM_ROTATION_ANGLE_270) ? w : h,
               .out.block_offset_x = x_offset,
               .out.block_offset_y = y_offset,
               .out.srm_cm = (LV_COLOR_DEPTH == 24) ? PPA_SRM_COLOR_MODE_RGB888 : PPA_SRM_COLOR_MODE_RGB565,

               .rotation_angle = ppa_rotation,
               .scale_x = 1.0,
               .scale_y = 1.0,
               .rgb_swap = 0,
               .byte_swap = 0,
               .mode = PPA_TRANS_MODE_BLOCKING,
        };
        ESP_ERROR_CHECK(ppa_do_scale_rotate_mirror(hw_resource.ppa_handle, &oper_config));
        return;
    }
#endif

    int deg = 0;
    switch (rotation) {
    case ESP_LV_ADAPTER_ROTATE_90:
        deg = 90;
        break;
    case ESP_LV_ADAPTER_ROTATE_180:
        deg = 180;
        break;
    case ESP_LV_ADAPTER_ROTATE_270:
        deg = 270;
        break;
    default:
        deg = 0;
        break;
    }
    if (deg != 0) {
        display_rotate_image(from,
                             to,
                             w,
                             h,
                             deg,
                             color_bytes,
                             impl->block_size_small,
                             impl->block_size_large);
    }
}

/**********************
 *   HELPER FUNCTIONS
 **********************/

/**
 * @brief Copy unrendered areas from front buffer to back buffer
 */
static void copy_unrendered_area_from_front_to_back(lv_disp_t *disp_refr, esp_lv_adapter_display_bridge_v8_t *impl)
{
    uint16_t hor_res = bridge_h_res(impl);
    uint16_t ver_res = bridge_v_res(impl);
    uint8_t color_bytes = bridge_color_bytes(impl);
    esp_lv_adapter_rotation_t rotation = bridge_rotation(impl);

    if (rotation != ESP_LV_ADAPTER_ROTATE_0) {
        for (int i = 0; i < disp_refr->inv_p; i++) {
            lv_area_t *a = &disp_refr->inv_areas[i];
            int x1, y1, x2, y2;
            display_coord_to_phy(a->x1, a->y1, &x1, &y1, rotation, hor_res, ver_res);
            display_coord_to_phy(a->x2, a->y2, &x2, &y2, rotation, hor_res, ver_res);
            a->x1 = LV_MIN(x1, x2);
            a->x2 = LV_MAX(x1, x2);
            a->y1 = LV_MIN(y1, y2);
            a->y2 = LV_MAX(y1, y2);
        }
    }

    typedef struct {
        int x1, y1, x2, y2;
    } rect_t;

    /* Step-1: Build "unsynced" list = FullScreen – Union(dirty) */
    rect_t unsync_rects[LV_INV_BUF_SIZE * 4 + 4];
    int    unsync_cnt = 1;
    unsync_rects[0] = (rect_t) {
        0, 0, hor_res - 1, ver_res - 1
    };

    for (int i = 0; i < disp_refr->inv_p; i++) {
        if (disp_refr->inv_area_joined[i]) {
            continue; /* skip already-joined areas */
        }

        rect_t d = {
            .x1 = disp_refr->inv_areas[i].x1,
            .y1 = disp_refr->inv_areas[i].y1,
            .x2 = disp_refr->inv_areas[i].x2,
            .y2 = disp_refr->inv_areas[i].y2,
        };

        int j = 0;
        while (j < unsync_cnt) {
            rect_t r = unsync_rects[j];

            /* no intersection => keep going */
            bool overlap = !(d.x2 < r.x1 || d.x1 > r.x2 || d.y2 < r.y1 || d.y1 > r.y2);
            if (!overlap) {
                j++;
                continue;
            }

            /* remove r from list */
            for (int k = j; k < unsync_cnt - 1; k++) {
                unsync_rects[k] = unsync_rects[k + 1];
            }
            unsync_cnt--;

            /* split r − d (up to four rectangles) and push back */
            if (d.y1 > r.y1) { /* top slice */
                unsync_rects[unsync_cnt++] = (rect_t) {
                    r.x1, r.y1, r.x2, d.y1 - 1
                };
            }
            if (d.y2 < r.y2) { /* bottom slice */
                unsync_rects[unsync_cnt++] = (rect_t) {
                    r.x1, d.y2 + 1, r.x2, r.y2
                };
            }

            int overlap_y1 = LV_MAX(r.y1, d.y1);
            int overlap_y2 = LV_MIN(r.y2, d.y2);

            if (d.x1 > r.x1) { /* left slice */
                unsync_rects[unsync_cnt++] = (rect_t) {
                    r.x1, overlap_y1, d.x1 - 1, overlap_y2
                };
            }
            if (d.x2 < r.x2) { /* right slice */
                unsync_rects[unsync_cnt++] = (rect_t) {
                    d.x2 + 1, overlap_y1, r.x2, overlap_y2
                };
            }
        }
    }

    if (unsync_cnt == 0) {
        return; /* whole screen was rendered this frame */
    }

    /* Step-2: Merge rectangles which share one axis span to reduce copy */
    bool merged;
    do {
        merged = false;
        for (int i = 0; i < unsync_cnt; i++) {
            for (int j = i + 1; j < unsync_cnt; j++) {
                rect_t *a = &unsync_rects[i];
                rect_t *b = &unsync_rects[j];

                /* Horizontal merge: identical vertical span, x ranges touch/overlap */
                if (a->y1 == b->y1 && a->y2 == b->y2 &&
                        b->x1 <= a->x2 + 1 && b->x2 >= a->x1 - 1) {
                    a->x1 = LV_MIN(a->x1, b->x1);
                    a->x2 = LV_MAX(a->x2, b->x2);

                    for (int k = j; k < unsync_cnt - 1; k++) {
                        unsync_rects[k] = unsync_rects[k + 1];
                    }
                    unsync_cnt--;
                    merged = true;
                    goto MERGE_RESTART; /* restart outer loops */
                }

                /* Vertical merge: identical horizontal span, y ranges touch/overlap */
                if (a->x1 == b->x1 && a->x2 == b->x2 &&
                        b->y1 <= a->y2 + 1 && b->y2 >= a->y1 - 1) {
                    a->y1 = LV_MIN(a->y1, b->y1);
                    a->y2 = LV_MAX(a->y2, b->y2);

                    for (int k = j; k < unsync_cnt - 1; k++) {
                        unsync_rects[k] = unsync_rects[k + 1];
                    }
                    unsync_cnt--;
                    merged = true;
                    goto MERGE_RESTART;
                }
            }
        }
MERGE_RESTART:;
    } while (merged);

    /* Step-3: Copy using DMA2D when available, fallback to CPU memcpy */
#if SOC_DMA2D_SUPPORTED

    if (unsync_cnt > 0) {
        display_cache_msync_framebuffer(impl->front_fb, impl->runtime.frame_buffer_size);
    }

    for (int idx = 0; idx < unsync_cnt; idx++) {
        rect_t r = unsync_rects[idx];
        size_t copy_w_px = r.x2 - r.x1 + 1;
        size_t copy_h_px = r.y2 - r.y1 + 1;

        int offset_x = r.x1;

        esp_async_fbcpy_trans_desc_t tr = {
            .src_buffer        = impl->front_fb,
            .dst_buffer        = impl->back_fb,
            .src_buffer_size_x = hor_res,
            .src_buffer_size_y = ver_res,
            .dst_buffer_size_x = hor_res,
            .dst_buffer_size_y = ver_res,
            .src_offset_x      = offset_x,
            .src_offset_y      = r.y1,
            .dst_offset_x      = offset_x,
            .dst_offset_y      = r.y1,
            .copy_size_x       = copy_w_px,
            .copy_size_y       = copy_h_px,
            .pixel_format_unique_id = {
                .color_type_id = (color_bytes == 2) ?
                COLOR_TYPE_ID(COLOR_SPACE_RGB, COLOR_PIXEL_RGB565) :
                COLOR_TYPE_ID(COLOR_SPACE_RGB, COLOR_PIXEL_RGB888)
            },
        };

        /* submit and wait */
        ESP_ERROR_CHECK(display_bridge_dma2d_copy_sync(&tr, portMAX_DELAY));
    }
#else /* !SOC_DMA2D_SUPPORTED */

    const int bytes_per_pixel = color_bytes;
    for (int idx = 0; idx < unsync_cnt; idx++) {
        rect_t r = unsync_rects[idx];
        int width_px        = r.x2 - r.x1 + 1;
        int bytes_per_line  = width_px * bytes_per_pixel;
        uint8_t *src_line = (uint8_t *)impl->front_fb + (r.y1 * hor_res + r.x1) * bytes_per_pixel;
        uint8_t *dst_line = (uint8_t *)impl->back_fb  + (r.y1 * hor_res + r.x1) * bytes_per_pixel;

        for (int y = r.y1; y <= r.y2; y++) {
            memcpy(dst_line, src_line, bytes_per_line);
            src_line += hor_res * bytes_per_pixel;
            dst_line += hor_res * bytes_per_pixel;
        }
    }

#endif /* !SOC_DMA2D_SUPPORTED */
}

/**
 * @brief Save current dirty region for later use
 */
static void flush_dirty_save(esp_lv_adapter_display_dirty_region_t *dirty_area)
{
    lv_disp_t *disp = _lv_refr_get_disp_refreshing();
    if (!disp) {
        display_dirty_region_reset(dirty_area);
        return;
    }

    display_dirty_region_capture(dirty_area,
                                 disp->inv_areas,
                                 disp->inv_area_joined,
                                 disp->inv_p);
}

/**
 * @brief Probe flush type to determine copy strategy (thread-safe)
 *
 * Thread-safe: Uses per-display prev_flush_status instead of global static
 */
static esp_lv_adapter_display_flush_probe_t flush_copy_probe(lv_disp_drv_t *drv, lv_disp_t *disp)
{
    esp_lv_adapter_display_flush_status_t cur_status;
    esp_lv_adapter_display_flush_probe_t probe_result;
    lv_disp_t *disp_refr = _lv_refr_get_disp_refreshing();

    /* Get display node for per-display state */
    esp_lv_adapter_display_node_t *node = drv->user_data;
    if (!node) {
        return ESP_LV_ADAPTER_DISPLAY_FLUSH_PROBE_PART_COPY;
    }

    uint32_t flush_ver = 0;
    uint32_t flush_hor = 0;
    for (int i = 0; i < disp_refr->inv_p; i++) {
        if (disp_refr->inv_area_joined[i] == 0) {
            flush_ver = (disp_refr->inv_areas[i].y2 + 1 - disp_refr->inv_areas[i].y1);
            flush_hor = (disp_refr->inv_areas[i].x2 + 1 - disp_refr->inv_areas[i].x1);
            break;
        }
    }

    cur_status = ((flush_ver == disp->driver->ver_res) && (flush_hor == disp->driver->hor_res)) ?
                 ESP_LV_ADAPTER_DISPLAY_FLUSH_STATUS_FULL : ESP_LV_ADAPTER_DISPLAY_FLUSH_STATUS_PART;

    if (node->prev_flush_status == ESP_LV_ADAPTER_DISPLAY_FLUSH_STATUS_FULL) {
        if (cur_status == ESP_LV_ADAPTER_DISPLAY_FLUSH_STATUS_PART) {
            probe_result = ESP_LV_ADAPTER_DISPLAY_FLUSH_PROBE_FULL_COPY;
        } else {
            probe_result = ESP_LV_ADAPTER_DISPLAY_FLUSH_PROBE_SKIP_COPY;
        }
    } else {
        probe_result = ESP_LV_ADAPTER_DISPLAY_FLUSH_PROBE_PART_COPY;
    }
    node->prev_flush_status = cur_status;

    return probe_result;
}

/**
 * @brief Copy dirty region with rotation
 */
static void flush_dirty_copy(esp_lv_adapter_display_bridge_v8_t *impl,
                             void *dst,
                             void *src,
                             esp_lv_adapter_display_dirty_region_t *dirty_area)
{
    esp_lv_adapter_rotation_t rotation = bridge_rotation(impl);
    uint8_t color_bytes = bridge_color_bytes(impl);

    for (int i = 0; i < dirty_area->inv_p; i++) {
        if (dirty_area->inv_area_joined[i] == 0) {
            lv_coord_t x_start = dirty_area->inv_areas[i].x1;
            lv_coord_t x_end   = dirty_area->inv_areas[i].x2;
            lv_coord_t y_start = dirty_area->inv_areas[i].y1;
            lv_coord_t y_end   = dirty_area->inv_areas[i].y2;

            rotate_copy_region(impl, src, dst,
                               x_start, y_start, x_end, y_end,
                               LV_HOR_RES, LV_VER_RES, rotation, color_bytes);
        }
    }
}

#endif /* LVGL_VERSION_MAJOR < 9 */
