/*
 * SPDX-FileCopyrightText: 2023-2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <stdint.h>

#include "driver/gpio.h"
#include "esp_err.h"
#include "esp_lcd_types.h"
#include "lvgl.h"

#if __has_include ("esp_lcd_touch.h")
#include "esp_lcd_touch.h"
#define ESP_LVGL_PORT_TOUCH_COMPONENT 1
#endif

#ifdef __cplusplus
extern "C" {
#endif

typedef bool (*lvgl_port_wait_cb)(void *handle);

/**
 * @brief Init configuration structure
 */
typedef struct {
    int task_priority;      /*!< LVGL task priority */
    int task_stack;         /*!< LVGL task stack size */
    int task_affinity;      /*!< LVGL task pinned to core (-1 is no affinity) */
    int task_max_sleep_ms;  /*!< Maximum sleep in LVGL task */
    int timer_period_ms;    /*!< LVGL timer tick period in ms */
} lvgl_port_cfg_t;

typedef struct {
    esp_lcd_panel_io_handle_t io_handle;    /*!< LCD panel IO handle */
    esp_lcd_panel_handle_t panel_handle;    /*!< LCD panel handle */
    lvgl_port_wait_cb draw_wait_cb;

    uint32_t    buffer_size;    /*!< Size of the buffer for the screen in pixels */
    uint32_t    trans_size;     /*!< Allocated buffer will be in SRAM to move framebuf */
    uint32_t    hres;           /*!< LCD display horizontal resolution */
    uint32_t    vres;           /*!< LCD display vertical resolution */
    lv_disp_rot_t   sw_rotate;    /* Panel software rotate_mask */
    struct {
        unsigned int buff_dma: 1;    /*!< Allocated LVGL buffer will be DMA capable */
        unsigned int buff_spiram: 1; /*!< Allocated LVGL buffer will be in PSRAM */
    } flags;
} lvgl_port_display_cfg_t;

#if __has_include ("esp_lcd_touch.h")
/**
 * @brief Configuration touch structure
 */
typedef struct {
    lv_disp_t *disp;    /*!< LVGL display handle (returned from lvgl_port_add_disp) */
    esp_lcd_touch_handle_t   handle;   /*!< LCD touch IO handle */

    lvgl_port_wait_cb touch_wait_cb;
} lvgl_port_touch_cfg_t;
#endif

/**
 * @brief LVGL port configuration structure
 *
 */
#define ESP_LVGL_PORT_INIT_CONFIG() \
    {                               \
        .task_priority = 4,       \
        .task_stack = 4096,       \
        .task_affinity = -1,      \
        .task_max_sleep_ms = 500, \
        .timer_period_ms = 5,     \
    }

/**
 * @brief Initialize LVGL portation
 *
 * @note This function initialize LVGL and create timer and task for LVGL right working.
 *
 * @return
 *      - ESP_OK                    on success
 *      - ESP_ERR_INVALID_ARG       if some of the create_args are not valid
 *      - ESP_ERR_INVALID_STATE     if esp_timer library is not initialized yet
 *      - ESP_ERR_NO_MEM            if memory allocation fails
 */
esp_err_t lvgl_port_init(const lvgl_port_cfg_t *cfg);

/**
 * @brief Deinitialize LVGL portation
 *
 * @note This function deinitializes LVGL and stops the task if running.
 * Some deinitialization will be done after the task will be stopped.
 *
 * @return
 *      - ESP_OK                    on success
 */
esp_err_t lvgl_port_deinit(void);

/**
 * @brief Add display handling to LVGL
 *
 * @note Allocated memory in this function is not free in deinit. You must call lvgl_port_remove_disp for free all memory!
 *
 * @param disp_cfg Display configuration structure
 * @return Pointer to LVGL display or NULL when error occurred
 */
lv_disp_t *lvgl_port_add_disp(const lvgl_port_display_cfg_t *disp_cfg);

/**
 * @brief Remove display handling from LVGL
 *
 * @note Free all memory used for this display.
 *
 * @return
 *      - ESP_OK                    on success
 */
esp_err_t lvgl_port_remove_disp(lv_disp_t *disp);

#ifdef ESP_LVGL_PORT_TOUCH_COMPONENT
/**
 * @brief Add LCD touch as an input device
 *
 * @note Allocated memory in this function is not free in deinit. You must call lvgl_port_remove_touch for free all memory!
 *
 * @param touch_cfg Touch configuration structure
 * @return Pointer to LVGL touch input device or NULL when error occurred
 */
lv_indev_t *lvgl_port_add_touch(const lvgl_port_touch_cfg_t *touch_cfg);

/**
 * @brief Remove selected LCD touch from input devices
 *
 * @note Free all memory used for this display.
 *
 * @return
 *      - ESP_OK                    on success
 */
esp_err_t lvgl_port_remove_touch(lv_indev_t *touch);
#endif

/**
 * @brief Take LVGL mutex
 *
 * @param[in] timeout_ms: Timeout in [ms]. 0 will block indefinitely.
 *
 * @return
 *      - true:  Mutex was taken
 *      - false: Mutex was NOT taken
 */
bool lvgl_port_lock(uint32_t timeout_ms);

/**
 * @brief Give LVGL mutex
 *
 */
void lvgl_port_unlock(void);

#ifdef __cplusplus
}
#endif
