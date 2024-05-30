/*
 * SPDX-FileCopyrightText: 2023-2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <stdint.h>

#include "driver/spi_common.h"
#include "esp_err.h"
#include "esp_lcd_panel_vendor.h"
#include "esp_lcd_types.h"
#include "freertos/FreeRTOS.h"
#include "soc/soc_caps.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * This structure is the configuration for ST77903 using QSPI interface.
 */
typedef struct {
    struct {
        spi_host_device_t host_id;  /*!< SPI host ID */
        int write_pclk_hz;          /*!< SPI clock frequency in Hz (normally use 40000000) */
        int read_pclk_hz;           /*!< SPI clock frequency in Hz (normally use 1000000) */
        int cs_io_num;              /*!< GPIO pin for LCD CS signal, set to -1 if not used */
        int sclk_io_num;            /*!< GPIO pin for LCD SCK(SCL) signal */
        int data0_io_num;           /*!< GPIO pin for LCD D0(SDA) signal */
        int data1_io_num;           /*!< GPIO pin for LCD D1 signal */
        int data2_io_num;           /*!< GPIO pin for LCD D2 signal */
        int data3_io_num;           /*!< GPIO pin for LCD D3 signal */
    } qspi;
    struct {
        uint8_t refresh_priority;   /*!< Priority of refresh task */
        uint32_t refresh_size;      /*!< Stack size of refresh task */
        int refresh_core;           /*!< Pined core of refresh task, set to `tskNO_AFFINITY` if not pin to any core */
        uint8_t load_priority;      /*!< Priority of load memory task */
        uint32_t load_size;         /*!< Stack size of load memory task */
        int load_core;              /*!< Pined core of load memory task, set to `tskNO_AFFINITY` if not pin to any core */
    } task;
    size_t fb_num;                  /*!< Number of screen-sized frame buffers that allocated by the driver.
                                      *  By default (set to either 0 or 1) only one frame buffer will be used.
                                      */
    size_t trans_pool_size;         /*!< Size of one transaction pool. Each pool contains multiple transactions which used to transfer color data.
                                      *  Each transaction contains one line of LCD frame buffer.
                                      *  It also decides the size of bounce buffer if `flags.fb_in_psram` is set to 1.
                                      */
    size_t trans_pool_num;          /*!< Number of transaction pool. Generally, it's recommended to set it to at least 2, and preferably to 3 */
    uint16_t hor_res;               /*!< Horizontal resolution of LCD */
    uint16_t ver_res;               /*!< Vertical resolution of LCD */
    struct {
        unsigned int fb_in_psram : 1;       /*<! If this flag is enabled, the frame buffer will be allocated from PSRAM, preferentially */
        unsigned int skip_init_host: 1;     /*<! If this flag is enabled, the driver will skip to initialize SPI bus and user should do it in advance */
        unsigned int enable_read_reg: 1;    /*<! If this flag is enabled, the driver will support to read register */
        unsigned int enable_cal_fps : 1;    /*<! If this flag is enabled, the refresh task will calculate FPS */
    } flags;
} st77903_qspi_config_t;

/**
 * @brief LCD panel initialization commands.
 *
 */
typedef struct {
    int cmd;                /*<! The specific LCD command */
    const void *data;       /*<! Buffer that holds the command specific data */
    size_t data_bytes;      /*<! Size of `data` in memory, in bytes */
    unsigned int delay_ms;  /*<! Delay in milliseconds after this command */
} st77903_lcd_init_cmd_t;

/**
 * @brief LCD panel vendor configuration.
 *
 * @note  This structure can be used to enable QSPI mode and override default initialization commands.
 * @note  This structure needs to be passed to the `vendor_config` field in `esp_lcd_panel_dev_config_t`.
 *
 */
typedef struct {
    const st77903_qspi_config_t *qspi_config;
    const st77903_lcd_init_cmd_t *init_cmds;    /*!< Pointer to initialization commands array. Set to NULL if using default commands.
                                                 *   The array should be declared as `static const` and positioned outside the function.
                                                 *   Please refer to `vendor_specific_init_default` in source file
                                                 */
    uint16_t init_cmds_size;                    /*<! Number of commands in above array */

    struct {
        unsigned int mirror_by_cmd: 1;          /*<! The `esp_lcd_panel_mirror()` function will be implemented by LCD command if set to 1.
                                                 *   Otherwise, the function will be implemented by software.
                                                 */
    } flags;
} st77903_vendor_config_t;

/**
 * @brief Create LCD panel for model ST77903
 *
 * @note  Vendor specific initialization can be different between manufacturers, should consult the LCD supplier for initialization sequence code.
 *
 * @param[in] io LCD panel IO handle
 * @param[in] panel_dev_config General panel device configuration (Use `vendor_config` to select interface type or override default initialization commands)
 * @param[out] ret_panel Returned LCD panel handle
 * @return
 *      - ESP_OK: Success
 *      - Otherwise: Fail
 */

/**
 * @brief QSPI configuration structure
 */
#define ST77903_QSPI_REFRESH_TASK_PRIO_DEFAULT      (23)
#define ST77903_QSPI_REFRESH_TASK_SIZE_DEFAULT      (3 * 1024)
#define ST77903_QSPI_REFRESH_TASK_CORE_DEFAULT      (tskNO_AFFINITY)

#define ST77903_QSPI_LOAD_TASK_PRIO_DEFAULT         (ST77903_QSPI_REFRESH_TASK_PRIO_DEFAULT)
#define ST77903_QSPI_LOAD_TASK_SIZE_DEFAULT         (3 * 1024)
#define ST77903_QSPI_LOAD_TASK_CORE_DEFAULT         (tskNO_AFFINITY)

#define ST77903_QSPI_TASK_CONFIG_DEFAULT()                          \
    {                                                               \
        .refresh_priority = ST77903_QSPI_REFRESH_TASK_PRIO_DEFAULT, \
        .refresh_size = ST77903_QSPI_REFRESH_TASK_SIZE_DEFAULT,     \
        .refresh_core = ST77903_QSPI_REFRESH_TASK_CORE_DEFAULT,     \
        .load_priority = ST77903_QSPI_LOAD_TASK_PRIO_DEFAULT,       \
        .load_size = ST77903_QSPI_LOAD_TASK_SIZE_DEFAULT,           \
        .load_core = ST77903_QSPI_LOAD_TASK_CORE_DEFAULT,           \
    }

#define ST77903_QSPI_WRITE_PCLK_HZ_DEFAULT           (40 * 1000 * 1000)
#define ST77903_QSPI_READ_PCLK_HZ_DEFAULT            (1 * 1000 * 1000)

#define ST77903_QSPI_CONFIG_DEFAULT(spi_host, cs, sck, d0, d1, d2, d3, fbs, h_res, v_res) \
    {                                                           \
        .qspi = {                                               \
            .host_id = spi_host,                                \
            .write_pclk_hz = ST77903_QSPI_WRITE_PCLK_HZ_DEFAULT,\
            .read_pclk_hz = ST77903_QSPI_READ_PCLK_HZ_DEFAULT,  \
            .cs_io_num = cs,                                    \
            .sclk_io_num = sck,                                 \
            .data0_io_num = d0,                                 \
            .data1_io_num = d1,                                 \
            .data2_io_num = d2,                                 \
            .data3_io_num = d3,                                 \
        },                                                      \
        .task = ST77903_QSPI_TASK_CONFIG_DEFAULT(),             \
        .fb_num = fbs,                                          \
        .trans_pool_size = 20,                                  \
        .trans_pool_num = 3,                                    \
        .hor_res = h_res,                                       \
        .ver_res = v_res,                                       \
        .flags = {                                              \
            .skip_init_host = 0,                                \
            .fb_in_psram = 1,                                   \
            .enable_read_reg = 0,                               \
            .enable_cal_fps = 0,                                \
        },                                                      \
    }

/**
 * @brief  Initialize ST77903 LCD panel with QSPI interface
 *
 * @param[in]  panel_dev_config LCD panel device configuration
 * @param[out] ret_panel LCD panel handle
 * @return
 *      - ESP_OK:    Success
 *      - Otherwise: Fail
 */
esp_err_t esp_lcd_new_panel_st77903_qspi(const esp_lcd_panel_dev_config_t *panel_dev_config, esp_lcd_panel_handle_t *ret_panel);

/**
 * Here are some specific functions for ST77903 using QSPI interface.
 */
/**
 * @brief Type of QSPI LCD panel event data
 */
typedef struct {
} st77903_qspi_event_data_t;

/**
 * @brief QSPI VSYNC event callback prototype
 *
 * @param[in] handle LCD panel handle
 * @param[in] edata Panel event data, fed by driver
 * @param[in] user_ctx User data, passed from `esp_lcd_new_panel_st77903()`
 * @return Whether a high priority task has been waken up by this function
 */
typedef bool (*st77903_qspi_vsync_cb_t)(esp_lcd_panel_handle_t handle, const st77903_qspi_event_data_t *edata, void *user_ctx);

/**
 * @brief Prototype for the function to be called when the bounce buffer finish copying the entire frame.
 *
 * @param[in] handle LCD panel handle, returned from `esp_lcd_new_panel_st77903()`
 * @param[in] edata Panel event data, fed by driver
 * @param[in] user_ctx User data, passed from `st77903_qspi_register_event_callbacks()`
 * @return Whether a high priority task has been waken up by this function
 */
typedef bool (*st77903_qspi_bounce_buf_finish_cb_t)(esp_lcd_panel_handle_t handle, const st77903_qspi_event_data_t *edata, void *user_ctx);

/**
 * @brief Group of supported ST77903 QSPI LCD panel callbacks
 * @note The callbacks are all running under ISR environment
 * @note When `CONFIG_LCD_ST77903_ISR_IRAM_SAFE` is enabled, the callback itself and functions called by it should be placed in IRAM.
 */
typedef struct {
    st77903_qspi_vsync_cb_t on_vsync;                             /*!< VSYNC event callback */
    st77903_qspi_bounce_buf_finish_cb_t on_bounce_frame_finish;   /*!< Bounce buffer finish callback. */
} st77903_qspi_event_callbacks_t;

/**
 * @brief Register ST77903 QSPI LCD panel event callbacks
 *
 * @param[in] handle LCD panel handle, returned from `esp_lcd_new_panel_st77903()`
 * @param[in] callbacks Group of callback functions
 * @param[in] user_ctx User data, which will be passed to the callback functions directly
 * @return
 *      - ESP_OK: Set event callbacks successfully
 *      - ESP_ERR_INVALID_ARG: Set event callbacks failed because of invalid argument
 *      - ESP_FAIL: Set event callbacks failed because of other error
 */
esp_err_t esp_lcd_st77903_qspi_register_event_callbacks(esp_lcd_panel_handle_t handle, const st77903_qspi_event_callbacks_t *callbacks, void *user_ctx);

/**
 * @brief Get the current FPS of the ST77903 QSPI LCD panel
 *
 * @note  This function is only available when `enable_cal_fps` flag is set to 1.
 *
 * @param[in]  handle LCD panel handle, returned from `esp_lcd_new_st77903_qspi_panel()`
 * @param[out] fps Pointer to store the FPS
 * @return
 *      - ESP_OK: Get FPS successfully
 *      - ESP_ERR_INVALID_ARG: Get FPS failed because of invalid argument
 *      - ESP_ERR_INVALID_STATE: Get FPS failed because of the `enable_cal_fps` flag is not set to 1
 */
esp_err_t esp_lcd_st77903_qspi_get_fps(esp_lcd_panel_handle_t handle, uint8_t *fps);

/**
 * @brief Get the address of the frame buffer(s) that allocated by the driver
 *
 * @param[in] handle  LCD panel handle, returned from `esp_lcd_new_st77903_qspi_panel()`
 * @param[in] fb_num Number of frame buffer(s) to get. This value must be the same as the number of the following parameters.
 * @param[out] fb0   Returned address of the frame buffer 0
 * @param[out] ...   List of other frame buffer addresses
 * @return
 *      - ESP_OK:              Success
 *      - ESP_ERR_INVALID_ARG: Fail
 */
esp_err_t esp_lcd_st77903_qspi_get_frame_buffer(esp_lcd_panel_handle_t handle, uint32_t fb_num, void **fb0, ...);

/**
 * @brief Read register from LCD panel
 *
 * @note  This function is only available when `enable_read_reg` flag is set to 1.
 *
 * @param[in]  handle LCD panel handle, returned from `esp_lcd_new_st77903_qspi_panel()`
 * @param[in]  reg Register address
 * @param[out] data Data buffer to store the read data
 * @param[in]  data_size Size of data buffer
 * @param[in]  timeout Timeout in ticks to wait for the transaction to complete, use `portMAX_DELAY` to wait forever.
 *      - ESP_OK:    Success
 *      - Otherwise: Fail
 */
esp_err_t esp_lcd_st77903_qspi_read_reg(esp_lcd_panel_handle_t handle, uint8_t reg, uint8_t *data, size_t data_size, TickType_t timeout);

#ifdef __cplusplus
}
#endif
