/*
 * SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <stdbool.h>
#include "sdkconfig.h"
#include "esp_err.h"
#include "esp_lv_adapter_display.h"
#include "esp_lv_adapter_input.h"

#if CONFIG_ESP_LVGL_ADAPTER_ENABLE_DECODER
#include "esp_lv_decoder.h"
#endif

#if CONFIG_ESP_LVGL_ADAPTER_ENABLE_FS
#include "esp_lv_fs.h"
#endif

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief LVGL adapter configuration structure
 */
typedef struct {
    uint32_t task_stack_size;       /*!< LVGL task stack size in bytes */
    uint32_t task_priority;         /*!< LVGL task priority */
    int task_core_id;               /*!< LVGL task core ID (-1 for no affinity) */
    uint32_t tick_period_ms;        /*!< LVGL tick period in milliseconds */
    uint32_t task_min_delay_ms;     /*!< Minimum LVGL task delay in milliseconds */
    uint32_t task_max_delay_ms;     /*!< Maximum LVGL task delay in milliseconds */
    bool stack_in_psram;            /*!< Allocate LVGL task stack in PSRAM when available */
} esp_lv_adapter_config_t;

/**
 * @brief Default configuration values for LVGL adapter
 * @{
 */
#define ESP_LV_ADAPTER_DEFAULT_STACK_SIZE        (8 * 1024)  /*!< Default task stack size (8KB) */
#define ESP_LV_ADAPTER_DEFAULT_TASK_PRIORITY     6           /*!< Default task priority */
#define ESP_LV_ADAPTER_DEFAULT_TASK_CORE_ID      (-1)        /*!< Default task core ID (-1 for no affinity) */
#define ESP_LV_ADAPTER_DEFAULT_TICK_PERIOD_MS    1           /*!< Default tick period in milliseconds */
#define ESP_LV_ADAPTER_DEFAULT_TASK_MIN_DELAY_MS 1           /*!< Default minimum task delay in milliseconds */
#define ESP_LV_ADAPTER_DEFAULT_TASK_MAX_DELAY_MS 15          /*!< Default maximum task delay in milliseconds */
/** @} */

/**
 * @brief Default configuration for LVGL adapter
 */
#define ESP_LV_ADAPTER_DEFAULT_CONFIG() {                            \
    .task_stack_size   = ESP_LV_ADAPTER_DEFAULT_STACK_SIZE,          \
    .task_priority     = ESP_LV_ADAPTER_DEFAULT_TASK_PRIORITY,       \
    .task_core_id      = ESP_LV_ADAPTER_DEFAULT_TASK_CORE_ID,        \
    .tick_period_ms    = ESP_LV_ADAPTER_DEFAULT_TICK_PERIOD_MS,      \
    .task_min_delay_ms = ESP_LV_ADAPTER_DEFAULT_TASK_MIN_DELAY_MS,   \
    .task_max_delay_ms = ESP_LV_ADAPTER_DEFAULT_TASK_MAX_DELAY_MS,   \
    .stack_in_psram    = false,                                      \
}

/*****************************************************************************
 *                         Core Lifecycle Functions                          *
 *****************************************************************************/

/**
 * @brief Initialize the LVGL adapter
 *
 * This function initializes the LVGL library and creates necessary resources.
 * Must be called before any other LVGL adapter functions.
 *
 * @param[in] config Pointer to LVGL adapter configuration
 *
 * @return
 *      - ESP_OK: Success
 *      - ESP_ERR_INVALID_STATE: Adapter already initialized
 *      - ESP_ERR_INVALID_ARG: Invalid configuration parameter
 *      - ESP_ERR_NO_MEM: Memory allocation failed
 */
esp_err_t esp_lv_adapter_init(const esp_lv_adapter_config_t *config);

/**
 * @brief Start the LVGL adapter task
 *
 * Creates and starts the LVGL task for handling timer events.
 *
 * @return
 *      - ESP_OK: Success
 *      - ESP_ERR_INVALID_STATE: Adapter not initialized
 *      - ESP_ERR_NO_MEM: Task creation failed
 */
esp_err_t esp_lv_adapter_start(void);

/**
 * @brief Deinitialize the LVGL adapter
 *
 * Stops the LVGL task and releases all resources.
 *
 * @return
 *      - ESP_OK: Success
 */
esp_err_t esp_lv_adapter_deinit(void);

/**
 * @brief Check if the LVGL adapter is initialized
 *
 * @return
 *      - true: Adapter is initialized
 *      - false: Adapter is not initialized
 */
bool esp_lv_adapter_is_initialized(void);

/*****************************************************************************
 *                         Thread Safety Functions                           *
 *****************************************************************************/

/**
 * @brief Acquire the LVGL adapter lock
 *
 * Must be called before accessing LVGL objects from user code.
 *
 * @param[in] timeout_ms Timeout in milliseconds, use -1 for infinite wait
 *
 * @return
 *      - ESP_OK: Lock acquired successfully
 *      - ESP_ERR_INVALID_STATE: Adapter not initialized
 *      - ESP_ERR_TIMEOUT: Failed to acquire lock within timeout
 */
esp_err_t esp_lv_adapter_lock(int32_t timeout_ms);

/**
 * @brief Release the LVGL adapter lock
 */
void esp_lv_adapter_unlock(void);

/*****************************************************************************
 *                         Display Control Functions                         *
 *****************************************************************************/

/**
 * @brief Force LVGL to refresh the display immediately
 *
 * This function triggers an immediate refresh of the LVGL display, forcing
 * all dirty areas to be redrawn without waiting for the next timer period.
 * Useful when you need to update the display synchronously.
 *
 * @param[in] disp Pointer to LVGL display object (NULL for default display)
 *
 * @return
 *      - ESP_OK: Success
 *      - ESP_ERR_INVALID_STATE: Adapter not initialized
 */
esp_err_t esp_lv_adapter_refresh_now(lv_display_t *disp);

/**
 * @brief Pause LVGL worker
 *
 * Stops the LVGL worker task and waits for acknowledgement. This API is intended
 * for advanced use cases requiring custom control over the LVGL rendering loop.
 *
 * @note You MUST pair this call with esp_lv_adapter_resume()
 * @note Do NOT call esp_lv_adapter_sleep_prepare() while manually paused
 * @note Handle timeout errors appropriately
 *
 * @param[in] timeout_ms Timeout in ms, -1 for infinite
 * @return
 *      - ESP_OK: Success
 *      - ESP_ERR_TIMEOUT: Timeout waiting for pause acknowledgement
 *      - ESP_ERR_INVALID_STATE: Adapter not initialized
 */
esp_err_t esp_lv_adapter_pause(int32_t timeout_ms);

/**
 * @brief Resume LVGL worker
 *
 * Resumes the LVGL worker task that was previously paused. Do not call this
 * if the adapter was paused by esp_lv_adapter_sleep_prepare().
 *
 * @return
 *      - ESP_OK: Success
 *      - ESP_ERR_INVALID_STATE: Adapter not initialized
 */
esp_err_t esp_lv_adapter_resume(void);

/**
 * @brief Prepare all displays for sleep
 *
 * Automatically detaches all LCD panels while preserving LVGL display objects
 * and UI state. This allows you to safely delete LCD hardware handles to release
 * DMA and framebuffer resources.
 *
 * @note This function automatically pauses the LVGL worker and waits for all
 *       pending flush operations to complete.
 * @note After calling this function, you can safely call esp_lcd_panel_del()
 *       for each display's panel handle.
 * @note Touch inputs remain registered. If you need to power down touch hardware,
 *       suspend it manually before calling this API.
 *
 * @return
 *      - ESP_OK: Success
 *      - ESP_ERR_INVALID_STATE: Adapter not initialized or already sleeping
 */
esp_err_t esp_lv_adapter_sleep_prepare(void);

/**
 * @brief Recover a display from sleep
 *
 * Re-binds a new LCD panel to an existing LVGL display, automatically updating
 * all framebuffer references. The UI elements are preserved.
 *
 * @note LCD hardware must be initialized before calling this function.
 * @note This function can be called multiple times to recover multiple displays.
 *       When all displays are recovered, the adapter automatically resumes the
 *       LVGL worker.
 * @note This function automatically triggers a full screen refresh for the display.
 *
 * @param[in] disp LVGL display handle to rebind
 * @param[in] panel New LCD panel handle
 * @param[in] panel_io New LCD panel IO handle (can be NULL for RGB/MIPI DSI)
 *
 * @return
 *      - ESP_OK: Success
 *      - ESP_ERR_INVALID_STATE: Adapter not initialized or not sleeping
 *      - ESP_ERR_INVALID_ARG: Invalid handles
 *      - ESP_FAIL: Failed to rebind panel
 */
esp_err_t esp_lv_adapter_sleep_recover(lv_display_t *disp,
                                       esp_lcd_panel_handle_t panel,
                                       esp_lcd_panel_io_handle_t panel_io);

/**
 * @brief Enable or disable dummy draw mode for a display
 *
 * In dummy draw mode, LVGL rendering is bypassed and the application takes direct
 * control of the display. Use esp_lv_adapter_dummy_draw_blit() to update the screen.
 *
 * @note Manages locking internally. Disabling triggers a full screen refresh.
 *
 * @param[in] disp Pointer to LVGL display object
 * @param[in] enable true to enable dummy draw, false to disable
 *
 * @return
 *      - ESP_OK: Success
 *      - ESP_ERR_INVALID_STATE: Adapter not initialized
 *      - ESP_ERR_INVALID_ARG: Invalid display pointer
 */
esp_err_t esp_lv_adapter_set_dummy_draw(lv_display_t *disp, bool enable);

/**
 * @brief Get the current dummy draw mode state for a display
 *
 * This function retrieves whether dummy draw mode is currently enabled.
 * In dummy draw mode, LVGL rendering is bypassed and the application
 * has direct control over the framebuffer.
 *
 * @param[in] disp Pointer to LVGL display object
 *
 * @return
 *      - true: Dummy draw mode is enabled
 *      - false: Dummy draw mode is disabled (or adapter not initialized/invalid display)
 */
bool esp_lv_adapter_get_dummy_draw_enabled(lv_display_t *disp);

/**
 * @brief Dummy draw callback collection
 *
 * All callbacks are optional; set any member to NULL to omit it.
 *
 * @note The on_vsync callback is executed in interrupt context on most panels,
 *       so implementations must be ISR-safe.
 */
typedef struct {
    void (*on_enable)(lv_display_t *disp, void *user_ctx);         /**< Called when dummy draw mode is enabled */
    void (*on_disable)(lv_display_t *disp, void *user_ctx);        /**< Called when dummy draw mode is disabled */
    void (*on_color_trans_done)(lv_display_t *disp, bool in_isr, void *user_ctx); /**< Called when color transfer is done */
    void (*on_vsync)(lv_display_t *disp, bool in_isr, void *user_ctx); /**< Called on VSYNC signal (ISR context) */
} esp_lv_adapter_dummy_draw_callbacks_t;

/**
 * @brief Register dummy draw callbacks for a display
 *
 * Callbacks allow applications to hook into dummy draw transitions and VSYNC
 * notifications (useful for self-refresh flows). Registering new callbacks
 * replaces any previously registered ones. Pass NULL to remove callbacks.
 *
 * @param disp     Display handle
 * @param cbs      Pointer to callback collection (NULL to clear)
 * @param user_ctx User context pointer passed back to callbacks
 *
 * @return
 *      - ESP_OK: Success
 *      - ESP_ERR_INVALID_STATE: Adapter not initialized
 *      - ESP_ERR_INVALID_ARG: Invalid display handle
 */
esp_err_t esp_lv_adapter_set_dummy_draw_callbacks(lv_display_t *disp,
                                                  const esp_lv_adapter_dummy_draw_callbacks_t *cbs,
                                                  void *user_ctx);

/**
 * @brief Blit a framebuffer region to display in dummy draw mode
 *
 * Directly sends framebuffer to display hardware, bypassing LVGL.
 *
 * @param[in] disp Pointer to LVGL display object
 * @param[in] x_start Starting X coordinate (inclusive)
 * @param[in] y_start Starting Y coordinate (inclusive)
 * @param[in] x_end Ending X coordinate (exclusive)
 * @param[in] y_end Ending Y coordinate (exclusive)
 * @param[in] frame_buffer Pointer to framebuffer data
 * @param[in] wait true to wait for completion, false to return immediately
 *
 * @return
 *      - ESP_OK: Success
 *      - ESP_ERR_INVALID_STATE: Adapter not initialized or dummy draw not enabled
 *      - ESP_ERR_INVALID_ARG: Invalid parameters
 */
esp_err_t esp_lv_adapter_dummy_draw_blit(lv_display_t *disp,
                                         int x_start,
                                         int y_start,
                                         int x_end,
                                         int y_end,
                                         const void *frame_buffer,
                                         bool wait);

#if CONFIG_ESP_LVGL_ADAPTER_ENABLE_FPS_STATS
/*****************************************************************************
 *                      FPS Statistics Functions                             *
 *****************************************************************************/

/**
 * @brief Enable or disable FPS statistics for a display
 *
 * When enabled, the adapter will track frames per second with minimal overhead
 * (< 2 microseconds per frame). Statistics are updated every 1 second.
 *
 * @param[in] disp Pointer to LVGL display object (NULL for default display)
 * @param[in] enable true to enable FPS statistics, false to disable
 *
 * @return
 *      - ESP_OK: Success
 *      - ESP_ERR_INVALID_STATE: Adapter not initialized
 *      - ESP_ERR_INVALID_ARG: Invalid display pointer
 */
esp_err_t esp_lv_adapter_fps_stats_enable(lv_display_t *disp, bool enable);

/**
 * @brief Get current FPS for a display
 *
 * Returns the current frames per second calculated over the last time window.
 * FPS is updated approximately every 1 second.
 *
 * @param[in] disp Pointer to LVGL display object (NULL for default display)
 * @param[out] fps Pointer to store the current FPS value (integer)
 *
 * @return
 *      - ESP_OK: Success
 *      - ESP_ERR_INVALID_STATE: Adapter not initialized or FPS not enabled
 *      - ESP_ERR_INVALID_ARG: Invalid parameters
 */
esp_err_t esp_lv_adapter_get_fps(lv_display_t *disp, uint32_t *fps);

/**
 * @brief Reset FPS statistics for a display
 *
 * Resets the frame counter and restarts the time window.
 * Useful when you want to measure FPS for a specific period.
 *
 * @param[in] disp Pointer to LVGL display object (NULL for default display)
 *
 * @return
 *      - ESP_OK: Success
 *      - ESP_ERR_INVALID_STATE: Adapter not initialized
 *      - ESP_ERR_INVALID_ARG: Invalid display pointer
 */
esp_err_t esp_lv_adapter_fps_stats_reset(lv_display_t *disp);
#endif /* CONFIG_ESP_LVGL_ADAPTER_ENABLE_FPS_STATS */

#if CONFIG_ESP_LVGL_ADAPTER_ENABLE_FS
/*****************************************************************************
 *                      File System Integration Functions                    *
 *****************************************************************************/

/**
 * @brief Mount a file system for use with LVGL
 *
 * Registers a file system driver with LVGL, allowing it to access files
 * using the configured drive letter.
 *
 * @param[in]  config     Pointer to file system configuration
 * @param[out] ret_handle Pointer to store the returned file system handle
 *
 * @return
 *      - ESP_OK: File system mounted successfully
 *      - ESP_ERR_INVALID_STATE: Adapter not initialized
 *      - ESP_ERR_INVALID_ARG: Invalid parameters
 *      - ESP_ERR_NO_MEM: Memory allocation failed
 */
esp_err_t esp_lv_adapter_fs_mount(const fs_cfg_t *config, esp_lv_fs_handle_t *ret_handle);

/**
 * @brief Unmount a file system from LVGL
 *
 * Unregisters a previously mounted file system and releases resources.
 *
 * @param[in] handle File system handle returned from esp_lv_adapter_fs_mount()
 *
 * @return
 *      - ESP_OK: File system unmounted successfully
 *      - ESP_ERR_INVALID_STATE: Adapter not initialized
 *      - ESP_ERR_INVALID_ARG: Invalid handle
 *      - ESP_ERR_NOT_FOUND: Handle not found in mounted list
 */
esp_err_t esp_lv_adapter_fs_unmount(esp_lv_fs_handle_t handle);
#endif

/*****************************************************************************
 *                      FreeType Font Functions (Optional)                   *
 *****************************************************************************/

#if CONFIG_ESP_LVGL_ADAPTER_ENABLE_FREETYPE

/* Compile-time check: Ensure LVGL FreeType support is enabled */
#if !defined(LV_USE_FREETYPE) || LV_USE_FREETYPE == 0
#error "============================================="
#error "  LVGL FreeType Support Not Enabled!"
#error "============================================="
#error "CONFIG_ESP_LVGL_ADAPTER_ENABLE_FREETYPE is enabled,"
#error "but LVGL's FreeType support is not configured."
#error ""
#error "Solution:"
#error "  Enable in menuconfig:"
#error "  Component config -> LVGL configuration"
#error "    -> [*] Use FreeType library"
#error ""
#error "  Or set in lv_conf.h:"
#error "  #define LV_USE_FREETYPE 1"
#error "============================================="
#endif

/**
 * @brief FreeType font handle
 *
 * Opaque handle for managing FreeType fonts
 */
typedef void *esp_lv_adapter_ft_font_handle_t;

/**
 * @brief FreeType font style enumeration
 */
typedef enum {
    ESP_LV_ADAPTER_FT_FONT_STYLE_NORMAL       = 0x00,    /*!< Normal style */
    ESP_LV_ADAPTER_FT_FONT_STYLE_ITALIC       = 0x01,    /*!< Italic style */
    ESP_LV_ADAPTER_FT_FONT_STYLE_BOLD         = 0x02,    /*!< Bold style */
    ESP_LV_ADAPTER_FT_FONT_STYLE_BOLD_ITALIC  = 0x03,    /*!< Bold and italic style */
} esp_lv_adapter_ft_font_style_t;

/**
 * @brief FreeType font configuration structure
 */
typedef struct {
    const char *name;                       /*!< Font name (file path or identification) */
    uint16_t size;                          /*!< Font size in pixels */
    esp_lv_adapter_ft_font_style_t style;         /*!< Font style */
    const void *mem;                        /*!< Pointer to font data in memory (NULL to load from file) */
    size_t mem_size;                        /*!< Size of font data (only used when mem is not NULL) */
} esp_lv_adapter_ft_font_config_t;

/**
 * @brief Initialize a FreeType font
 *
 * This function initializes a TrueType/OpenType font for use with LVGL.
 * The function is thread-safe and automatically acquires the LVGL lock.
 *
 * Font can be loaded from:
 * - Memory: Set mem and mem_size, name is used for identification only
 * - File: Set mem to NULL, name must be the full file path
 *
 * The font is automatically registered in the adapter's font list and will be
 * cleaned up when esp_lv_adapter_deinit() is called.
 *
 * @param[in] config Pointer to font configuration
 * @param[out] handle Pointer to store the font handle (can be NULL if not needed)
 *
 * @return
 *      - ESP_OK: Success
 *      - ESP_ERR_INVALID_STATE: Adapter not initialized
 *      - ESP_ERR_INVALID_ARG: Invalid configuration
 *      - ESP_ERR_NO_MEM: Memory allocation failed
 *      - ESP_FAIL: Font initialization failed
 *
 * @note The font handle can be used to retrieve the lv_font_t pointer or deinitialize the font
 * @note If handle is NULL, the font will still be initialized but cannot be managed individually
 */
esp_err_t esp_lv_adapter_ft_font_init(const esp_lv_adapter_ft_font_config_t *config,
                                      esp_lv_adapter_ft_font_handle_t *handle);

/**
 * @brief Get LVGL font pointer from FreeType font handle
 *
 * @param[in] handle FreeType font handle
 *
 * @return
 *      - lv_font_t pointer on success
 *      - NULL if handle is invalid
 */
const lv_font_t *esp_lv_adapter_ft_font_get(esp_lv_adapter_ft_font_handle_t handle);

/**
 * @brief Deinitialize a FreeType font
 *
 * This function deinitializes a FreeType font and releases its resources.
 * The function is thread-safe and automatically acquires the LVGL lock.
 *
 * @param[in] handle FreeType font handle
 *
 * @return
 *      - ESP_OK: Success
 *      - ESP_ERR_INVALID_STATE: Adapter not initialized
 *      - ESP_ERR_INVALID_ARG: Invalid handle
 *      - ESP_ERR_NOT_FOUND: Font not found in list
 *
 * @warning Do not use the font pointer after calling this function
 * @note This function is optional, fonts will be automatically cleaned up in esp_lv_adapter_deinit()
 */
esp_err_t esp_lv_adapter_ft_font_deinit(esp_lv_adapter_ft_font_handle_t handle);

/**
 * @brief Helper macro to create font configuration from file
 */
#define ESP_LV_ADAPTER_FT_FONT_FILE_CONFIG(file_path, font_size, font_style) {  \
    .name = file_path,                                                    \
    .size = font_size,                                                    \
    .style = font_style,                                                  \
    .mem = NULL,                                                          \
    .mem_size = 0,                                                        \
}

/**
 * @brief Helper macro to create font configuration from memory
 */
#define ESP_LV_ADAPTER_FT_FONT_MEM_CONFIG(font_name, font_size, font_style, mem_ptr, size_bytes) {  \
    .name = font_name,                                                                        \
    .size = font_size,                                                                        \
    .style = font_style,                                                                      \
    .mem = mem_ptr,                                                                           \
    .mem_size = size_bytes,                                                                   \
}

#endif /* CONFIG_ESP_LVGL_ADAPTER_ENABLE_FREETYPE */

#ifdef __cplusplus
}
#endif
