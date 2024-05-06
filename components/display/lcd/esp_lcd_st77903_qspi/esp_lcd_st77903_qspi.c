/*
 * SPDX-FileCopyrightText: 2023-2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdint.h>
#include <sys/param.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "freertos/queue.h"
#include "hal/dma_types.h"
#include "hal/spi_ll.h"
#include "rom/cache.h"
#include "soc/soc.h"
#include "driver/gpio.h"
#include "driver/spi_master.h"
#include "esp_check.h"
#include "esp_heap_caps.h"
#include "esp_lcd_panel_commands.h"
#include "esp_lcd_panel_interface.h"
#include "esp_lcd_panel_ops.h"
#include "esp_private/spi_common_internal.h"
#include "esp_private/spi_master_internal.h"
#include "esp_psram.h"
#include "esp_timer.h"
#include "sdkconfig.h"

#include "esp_lcd_st77903_qspi.h"

#define LCD_VSYNC_FRONT_NUM         (CONFIG_LCD_VSYNC_FRONT_NUM)
#define LCD_VSYNC_BACK_NUM          (CONFIG_LCD_VSYNC_BACK_NUM)

#define SPI_CMD_BITS                (8)
#define SPI_PARAM_BITS              (24)
#define SPI_MODE                    (0)

#define SPI_SEG_GAP_BASIC_US        (2)
#define SPI_SEG_GAP_GET_US(hor_res, bytes)  MAX(0, (int)(MAX(LCD_LINE_INTERVAL_MIN_US, (hor_res + 9) / 10) - \
                                                bytes / 20 - SPI_SEG_GAP_BASIC_US))
#define SPI_SEG_GAP_GET_CLK_LEN(time_us)    (time_us * (APB_CLK_FREQ / 1000000))

#define LCD_LINE_INTERVAL_MIN_US    (CONFIG_LCD_LINE_INTERVAL_MIN_US)

#define TASK_CHECK_TIME_MS          (CONFIG_LCD_TASK_CHECK_TIME_MS)
#define TASK_STOP_WAIT_TIME_MS      (CONFIG_LCD_TASK_STOP_WAIT_TIME_MS)
#define TASK_STOP_TIME_MAX_MS       (CONFIG_LCD_TASK_STOP_TIME_MAX_MS)
#define READ_WAIT_TIME_MAX_MS       (CONFIG_LCD_READ_WAIT_TIME_MAX_MS)

#define ST77903_INS_DATA            (0xDE)
#define ST77903_INS_READ            (0xDD)
#define ST77903_INS_CMD             (0xD8)
#define ST77903_CMD_HSYNC           (0x60)
#define ST77903_CMD_VSYNC           (0x61)
#define ST77903_CMD_BPC             (0xB5)
#define ST77903_CMD_DISCN           (0xB6)

typedef struct {
    int id;
} custom_spi_device_t ;

typedef struct {
    void *panel;
    bool is_color;
    bool is_vfp;
} custom_user_data_t;

typedef struct {
    esp_lcd_panel_t base;
    // SPI host
    spi_host_device_t spi_host_id;
    spi_device_handle_t spi_write_dev;
    spi_device_handle_t spi_read_dev;
    spi_multi_transaction_t **trans_pool;
    spi_multi_transaction_t vsync_front_pool[LCD_VSYNC_FRONT_NUM];
    spi_multi_transaction_t vsync_back_pool[LCD_VSYNC_BACK_NUM + 1];
    spi_multi_transaction_t write_cmd_seg;
    size_t trans_pool_size;
    size_t trans_pool_num;
    // Frame buffer
    void **fbs;
    size_t fb_num;
    size_t bits_per_pixel;
    size_t bytes_per_pixel;
    size_t bytes_per_line;
    size_t bytes_per_fb;
    uint16_t cur_fb_index;
    uint16_t next_fb_index;
    uint16_t load_line_cnt;
    uint16_t write_pool_index;
    SemaphoreHandle_t sem_count_free_trans;
    custom_user_data_t trans_user;
    custom_user_data_t vfp_user;
    custom_user_data_t vbp_user;
    void *user_ctx;                 // Reserved user's data of callback functions
    st77903_qspi_vsync_cb_t on_vsync;
    // Boucne buffer
    uint16_t load_bb_index;
    uint16_t cur_bb_fb_index;
    void **bbs;
    size_t bb_size;
    QueueHandle_t queue_load_mem_info;
    st77903_qspi_bounce_buf_finish_cb_t on_bounce_frame_finish;
    // Read register
    uint8_t read_reg;
    uint8_t *read_data;
    uint8_t read_data_size;
    SemaphoreHandle_t sem_read_ready;
    SemaphoreHandle_t sem_read_done;
    // Others
    int cs_io_num;
    int reset_gpio_num;
    struct {
        unsigned int enable_cal_fps : 1;
        unsigned int fb_in_psram : 1;
        unsigned int reset_level: 1;
        unsigned int skip_init_host: 1;
        unsigned int stop_refresh_task: 1;
        unsigned int stop_load_task: 1;
        unsigned int panel_need_reconfig: 1;
        unsigned int enable_read_reg: 1;
        unsigned int wait_frame_end: 1;
        unsigned int mirror_by_cmd: 1;
    } flags;
    struct {
        uint8_t refresh_priority;
        uint16_t refresh_size;
        int refresh_core;
        TaskHandle_t refresh_task_handle;
        uint8_t load_priority;
        uint16_t load_size;
        int load_core;
        TaskHandle_t load_task_handle;
    } task;
    uint8_t fps;
    uint16_t hor_res;
    uint16_t ver_res;
    const st77903_lcd_init_cmd_t *init_cmds;
    uint16_t init_cmds_size;
    int rotate_mask;    // Panel rotate_mask mask, Or'ed of `panel_rotate_mask_t`
    int x_gap;          // Extra gap in x coordinate, it's used when calculate the flush window
    int y_gap;          // Extra gap in y coordinate, it's used when calculate the flush window
    uint8_t madctl_val; // Save current value of LCD_CMD_MADCTL register
    uint8_t colmod_val; // Save current value of LCD_CMD_COLMOD register
} st77903_qspi_panel_t;

typedef struct {
    void *from;
    void *to;
    void *next_buf;
    uint32_t bytes;
    uint32_t next_buf_bytes;
} queue_load_mem_info_t;

#define PANEL_SWAP_XY  0
#define PANEL_MIRROR_Y 1
#define PANEL_MIRROR_X 2

typedef enum {
    ROTATE_MASK_SWAP_XY = BIT(PANEL_SWAP_XY),
    ROTATE_MASK_MIRROR_Y = BIT(PANEL_MIRROR_Y),
    ROTATE_MASK_MIRROR_X = BIT(PANEL_MIRROR_X),
} panel_rotate_mask_t;

static const char *TAG = "st77903.qspi";

static bool load_trans_pool(st77903_qspi_panel_t *panel, spi_multi_transaction_t *seg_trans);
static void post_trans_color_cb(spi_transaction_t *trans);
static esp_err_t lcd_cmd_config(st77903_qspi_panel_t *panel);
static esp_err_t lcd_write_color(st77903_qspi_panel_t *panel);
static esp_err_t lcd_write_cmd(st77903_qspi_panel_t *panel, uint8_t cmd, const void *param, size_t param_size);
static void load_memory_task(void *arg);
static void refresh_task(void *arg);
static esp_err_t stop_refresh(st77903_qspi_panel_t *panel);

static esp_err_t panel_st77903_qspi_init(esp_lcd_panel_t *handle);
static esp_err_t panel_st77903_qspi_reset(esp_lcd_panel_t *handle);
static esp_err_t panel_st77903_qspi_del(esp_lcd_panel_t *handle);
static esp_err_t panel_st77903_qspi_draw_bitmap(esp_lcd_panel_t *panel, int x_start, int y_start, int x_end, int y_end, const void *color_data);
static esp_err_t panel_st77903_qspi_mirror(esp_lcd_panel_t *handle, bool mirror_x, bool mirror_y);
static esp_err_t panel_st77903_qspi_swap_xy(esp_lcd_panel_t *handle, bool swap_axes);
static esp_err_t panel_st77903_qspi_set_gap(esp_lcd_panel_t *handle, int x_gap, int y_gap);
static esp_err_t panel_st77903_qspi_disp_on_off(esp_lcd_panel_t *handle, bool on_off);

static esp_err_t st77903_qspi_alloc_buffers(st77903_qspi_panel_t *panel)
{
    // fb_in_psram is only an option, if there's no PSRAM on board, we fallback to alloc from SRAM
    bool fb_in_psram = false;
    if (panel->flags.fb_in_psram) {
#if CONFIG_SPIRAM_USE_MALLOC || SPIRAM_USE_MALLOC
        if (esp_psram_is_initialized()) {
            fb_in_psram = true;
        }
#endif
    }

    // Alloc frame buffer
    panel->fbs = (void **)heap_caps_malloc(panel->fb_num * sizeof(void *), MALLOC_CAP_INTERNAL);
    for (int i = 0; i < panel->fb_num; i++) {
        if (fb_in_psram) {
            panel->fbs[i] = heap_caps_calloc(1, panel->bytes_per_fb, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
        } else {
            panel->fbs[i] = heap_caps_calloc(1, panel->bytes_per_fb, MALLOC_CAP_INTERNAL | MALLOC_CAP_DMA);
        }
        ESP_RETURN_ON_FALSE(panel->fbs[i], ESP_ERR_NO_MEM, TAG, "No mem for frame buffer(%dKB)", panel->bytes_per_fb / 1024);
    }
    panel->cur_fb_index = 0;
    panel->next_fb_index = 0;
    panel->flags.fb_in_psram = fb_in_psram;
    ESP_LOGD(TAG, "Frame buffer size: %zu, total: %zuKB", panel->bytes_per_fb, panel->fb_num * panel->bytes_per_fb / 1024);

    // Alloc bounce buffer
    if (panel->bb_size) {
        panel->bbs = (void **)heap_caps_malloc(panel->trans_pool_num * sizeof(void *), MALLOC_CAP_INTERNAL);
        for (int i = 0; i < panel->trans_pool_num; i++) {
            panel->bbs[i] = heap_caps_aligned_calloc(32, 1, panel->bb_size, MALLOC_CAP_INTERNAL | MALLOC_CAP_DMA);
            ESP_RETURN_ON_FALSE(panel->bbs[i], ESP_ERR_NO_MEM, TAG, "No mem for bounce buffer(%d, need %dKB)",
                                panel->trans_pool_num, panel->trans_pool_num * panel->bb_size / 1024);
        }
        ESP_LOGD(TAG, "Bounce buffer size: %zu, total: %zuKB", panel->bb_size, panel->trans_pool_num * panel->bb_size / 1024);
        panel->cur_bb_fb_index = 0;
    }

    // Alloc trans pool
    panel->trans_pool = (spi_multi_transaction_t **)heap_caps_malloc(panel->trans_pool_num * sizeof(spi_multi_transaction_t *), MALLOC_CAP_INTERNAL);
    uint16_t trans_pool_bytes = panel->trans_pool_size * sizeof(spi_transaction_t);
    for (int i = 0; i < panel->trans_pool_num; i++) {
        panel->trans_pool[i] = (spi_multi_transaction_t *)heap_caps_calloc(panel->trans_pool_size, sizeof(spi_multi_transaction_t), MALLOC_CAP_DMA);
        ESP_RETURN_ON_FALSE(panel->trans_pool[i], ESP_ERR_NO_MEM, TAG, "No mem for trans pool(%d, need %dKB)",
                            panel->trans_pool_size, trans_pool_bytes / 1024);
    }
    ESP_LOGD(TAG, "Trans pool size: %zu, total: %zuKB", panel->trans_pool_size, panel->trans_pool_num * trans_pool_bytes / 1024);

    // Init the constant data for trans pool
    panel->trans_user.panel = (void *)panel;
    panel->trans_user.is_color = true;
    spi_multi_transaction_t trans_pool_temp = {
        .base = {
            .cmd = ST77903_INS_DATA,
            .length = panel->bytes_per_line * 8,
            .addr = (((uint32_t)ST77903_CMD_HSYNC) << 8),
            .flags = SPI_TRANS_MODE_QIO,
            .user = (void *) &panel->trans_user,
        },
        .sct_gap_len = SPI_SEG_GAP_GET_CLK_LEN(SPI_SEG_GAP_GET_US(panel->hor_res, panel->bytes_per_line)),
    };
    ESP_LOGD(TAG, "segment_gap_clock_len: %ld", trans_pool_temp.sct_gap_len);

    for (int i = 0; i < panel->trans_pool_num; i++) {
        for (int j = 0; j < panel->trans_pool_size; j++) {
            memcpy(&panel->trans_pool[i][j], &trans_pool_temp, sizeof(spi_multi_transaction_t));
        }
    }

    // Init the constant data for vsync pool
    panel->vbp_user.panel = (void *)panel;
    panel->vbp_user.is_color = false;
    panel->vbp_user.is_vfp = false;
    spi_multi_transaction_t vsync_pool_temp = {
        .base = {
            .cmd = ST77903_INS_CMD,
            .length = 0,
            .addr = ((uint32_t)ST77903_CMD_HSYNC) << 8,
                                                  .user = (void *) &panel->vbp_user,
        },
        .sct_gap_len = SPI_SEG_GAP_GET_CLK_LEN(LCD_LINE_INTERVAL_MIN_US),
    };
    for (int i = 0; i < LCD_VSYNC_BACK_NUM + 1; i++) {
        memcpy(&panel->vsync_back_pool[i], &vsync_pool_temp, sizeof(spi_multi_transaction_t));
    }
    panel->vsync_back_pool[0].base.addr = ((uint32_t)ST77903_CMD_VSYNC) << 8;

    panel->vfp_user.panel = (void *)panel;
    panel->vfp_user.is_color = false;
    panel->vfp_user.is_vfp = true;
    vsync_pool_temp.base.user = (void *)&panel->vfp_user;
    for (int i = 0; i < LCD_VSYNC_FRONT_NUM; i++) {
        memcpy(&panel->vsync_front_pool[i], &vsync_pool_temp, sizeof(spi_multi_transaction_t));
    }

    return ESP_OK;
}

static int get_spi_device_id(spi_device_handle_t dev)
{
    return ((custom_spi_device_t *)dev)->id;
}

esp_err_t esp_lcd_new_panel_st77903_qspi(const esp_lcd_panel_dev_config_t *panel_dev_config, esp_lcd_panel_handle_t *ret_panel)
{
    ESP_RETURN_ON_FALSE(panel_dev_config && ret_panel, ESP_ERR_INVALID_ARG, TAG, "Invalid arguments");
    st77903_vendor_config_t *vendor_config = (st77903_vendor_config_t *)panel_dev_config->vendor_config;

    ESP_RETURN_ON_FALSE(vendor_config, ESP_ERR_INVALID_ARG, TAG, "`vendor_config` is necessary");
    const st77903_qspi_config_t *config = vendor_config->qspi_config;
    ESP_RETURN_ON_FALSE(config->trans_pool_size && config->trans_pool_num, ESP_ERR_INVALID_ARG, TAG, "Trans pool size and num must > 0");
    ESP_RETURN_ON_FALSE(config->hor_res && config->ver_res, ESP_ERR_INVALID_ARG, TAG, "Screen resolution must > 0");

    esp_err_t ret = ESP_OK;
    st77903_qspi_panel_t *panel = NULL;
    size_t bytes_per_pixel = ((panel_dev_config->bits_per_pixel > 16) ? 24 : 16) / 8;
    size_t dma_nodes_per_spi_trans =  1 + lldesc_get_required_num(bytes_per_pixel * config->hor_res);

    // Init SPI bus if necessary
    if (!config->flags.skip_init_host) {
        spi_bus_config_t spi_config = {
            .sclk_io_num = config->qspi.sclk_io_num,
            .data0_io_num = config->qspi.data0_io_num,
            .data1_io_num = config->qspi.data1_io_num,
            .data2_io_num = config->qspi.data2_io_num,
            .data3_io_num = config->qspi.data3_io_num,
            .flags = SPICOMMON_BUSFLAG_QUAD,
            .max_transfer_sz = config->trans_pool_size * config->trans_pool_num * dma_nodes_per_spi_trans *
            DMA_DESCRIPTOR_BUFFER_MAX_SIZE * 2,
        };
        ESP_RETURN_ON_ERROR(spi_bus_initialize(config->qspi.host_id, &spi_config, SPI_DMA_CH_AUTO), TAG, "SPI bus init failed");
        ESP_LOGD(TAG, "SPI max transfer size: %dKB", spi_config.max_transfer_sz / 1024);
        ESP_LOGD(TAG, "Init SPI bus[%d]", config->qspi.host_id);
    }

    spi_device_handle_t spi_read_device = NULL;
    if (config->flags.enable_read_reg) {
        spi_device_interface_config_t dev_config = {
            .command_bits = SPI_CMD_BITS,
            .address_bits = SPI_PARAM_BITS,
            .mode = SPI_MODE,
            .clock_speed_hz = config->qspi.read_pclk_hz,
            .spics_io_num = config->qspi.cs_io_num,
            .queue_size = 2,
            .flags = SPI_DEVICE_HALFDUPLEX | SPI_DEVICE_3WIRE,
        };
        ESP_GOTO_ON_ERROR(spi_bus_add_device((spi_host_device_t)config->qspi.host_id, &dev_config, &spi_read_device), err,
                          TAG, "Create spi read device failed");
        ESP_LOGD(TAG, "Create SPI read device[%d]", get_spi_device_id(spi_read_device));
    }

    // Create SPI device which is used to write data
    spi_device_handle_t spi_write_device = NULL;
    spi_device_interface_config_t dev_config = {
        .command_bits = SPI_CMD_BITS,
        .address_bits = SPI_PARAM_BITS,
        .mode = SPI_MODE,
        .clock_speed_hz = config->qspi.write_pclk_hz,
        .spics_io_num = config->qspi.cs_io_num,
        .queue_size = config->trans_pool_num,
        .flags = SPI_DEVICE_HALFDUPLEX | SPI_DEVICE_NO_RETURN_RESULT,
        .post_cb = post_trans_color_cb,
    };
    ESP_GOTO_ON_ERROR(spi_bus_add_device((spi_host_device_t)config->qspi.host_id, &dev_config, &spi_write_device), err,
                      TAG, "Create spi write device failed");
    ESP_GOTO_ON_ERROR(spi_bus_multi_trans_mode_enable(spi_write_device, true), err, TAG, "Segment mode enable failed");
    ESP_LOGD(TAG, "Create SPI write device[%d]", get_spi_device_id(spi_write_device));

    if (panel_dev_config->reset_gpio_num >= 0) {
        gpio_config_t io_conf = {
            .mode = GPIO_MODE_OUTPUT,
            .pin_bit_mask = 1ULL << panel_dev_config->reset_gpio_num,
        };
        ESP_GOTO_ON_ERROR(gpio_config(&io_conf), err, TAG, "Configure GPIO for RST line failed");
    }

    panel = (st77903_qspi_panel_t *)heap_caps_calloc(1, sizeof(st77903_qspi_panel_t), MALLOC_CAP_DMA | MALLOC_CAP_INTERNAL);
    ESP_GOTO_ON_FALSE(panel, ESP_ERR_NO_MEM, err, TAG, "Malloc failed");

    switch (panel_dev_config->rgb_ele_order) {
    case LCD_RGB_ELEMENT_ORDER_RGB:
        panel->madctl_val = 0;
        break;
    case LCD_RGB_ELEMENT_ORDER_BGR:
        panel->madctl_val |= LCD_CMD_BGR_BIT;
        break;
    default:
        ESP_GOTO_ON_FALSE(false, ESP_ERR_NOT_SUPPORTED, err, TAG, "unsupported color element order");
        break;
    }

    switch (panel_dev_config->bits_per_pixel) {
    case 16: // RGB565
        panel->colmod_val = 0x05;
        break;
    case 18: // RGB666
        panel->colmod_val = 0x06;
        break;
    case 24: // RGB888
        panel->colmod_val = 0x07;
        break;
    default:
        ESP_GOTO_ON_FALSE(false, ESP_ERR_NOT_SUPPORTED, err, TAG, "Unsupported pixel width");
        break;
    }

    // SPI host
    panel->spi_host_id = config->qspi.host_id;
    panel->spi_write_dev = spi_write_device;
    panel->spi_read_dev = spi_read_device;
    panel->trans_pool_size = config->trans_pool_size;
    panel->trans_pool_num = config->trans_pool_num;
    // Frame buffer
    panel->fb_num = (config->fb_num == 0) ? 1 : config->fb_num;
    panel->bits_per_pixel = panel_dev_config->bits_per_pixel;
    panel->bytes_per_pixel = bytes_per_pixel;
    panel->bytes_per_line = config->hor_res * bytes_per_pixel;
    panel->bytes_per_fb = panel->bytes_per_line * config->ver_res;
    panel->load_line_cnt = 0;
    panel->write_pool_index = 0;
    // Bounce buffer
    panel->load_bb_index = 0;
    if (config->flags.fb_in_psram) {
        panel->bb_size = panel->trans_pool_size * panel->bytes_per_line;
    }
    // Others
    panel->cs_io_num = config->qspi.cs_io_num;
    panel->reset_gpio_num = panel_dev_config->reset_gpio_num;
    panel->flags.enable_cal_fps = config->flags.enable_cal_fps;
    panel->flags.fb_in_psram = config->flags.fb_in_psram;
    panel->flags.reset_level = panel_dev_config->flags.reset_active_high;
    panel->flags.skip_init_host = config->flags.skip_init_host;
    panel->flags.stop_refresh_task = 1;
    panel->flags.stop_load_task = 1;
    panel->flags.panel_need_reconfig = 1;
    panel->flags.enable_read_reg = config->flags.enable_read_reg;
    panel->flags.wait_frame_end = 0;
    panel->flags.mirror_by_cmd = vendor_config->flags.mirror_by_cmd;
    panel->task.refresh_priority = config->task.refresh_priority;
    panel->task.refresh_core = config->task.refresh_core;
    panel->task.refresh_size = config->task.refresh_size;
    panel->task.load_priority = config->task.load_priority;
    panel->task.load_core = config->task.load_core;
    panel->task.load_size = config->task.load_size;
    panel->hor_res = config->hor_res;
    panel->ver_res = config->ver_res;
    panel->init_cmds = vendor_config->init_cmds;
    panel->init_cmds_size = vendor_config->init_cmds_size;
    panel->base.init = panel_st77903_qspi_init;
    panel->base.draw_bitmap = panel_st77903_qspi_draw_bitmap;
    panel->base.reset = panel_st77903_qspi_reset;
    panel->base.del = panel_st77903_qspi_del;
    panel->base.mirror = panel_st77903_qspi_mirror;
    panel->base.swap_xy = panel_st77903_qspi_swap_xy;
    panel->base.set_gap = panel_st77903_qspi_set_gap;
    panel->base.disp_on_off = panel_st77903_qspi_disp_on_off;

    ESP_GOTO_ON_ERROR(st77903_qspi_alloc_buffers(panel), err, TAG, "Alloc buffers failed");

    // Create semaphore for refresh task
    panel->sem_count_free_trans = xSemaphoreCreateCounting(panel->trans_pool_num, 0);
    ESP_RETURN_ON_FALSE(panel->sem_count_free_trans, ESP_ERR_NO_MEM, TAG, "No mem for refresh semaphore");
    // Create queue for load task
    if (panel->flags.fb_in_psram) {
        panel->queue_load_mem_info = xQueueCreate(panel->trans_pool_num, sizeof(queue_load_mem_info_t));
        ESP_RETURN_ON_FALSE(panel->queue_load_mem_info, ESP_ERR_NO_MEM, TAG, "No mem for queue load mem info");
    }

    if (panel->flags.enable_read_reg) {
        // Create semaphore for reading register
        panel->sem_read_ready = xSemaphoreCreateBinary();
        panel->sem_read_done = xSemaphoreCreateBinary();
        ESP_RETURN_ON_FALSE(panel->sem_read_ready && panel->sem_read_done, ESP_ERR_NO_MEM, TAG, "No mem for read semaphore");
    }

    *ret_panel = &panel->base;
    ESP_LOGD(TAG, "Create panel @%p", panel);

    return ESP_OK;

err:
    if (spi_write_device) {
        spi_bus_remove_device(spi_write_device);
    }
    if (spi_read_device) {
        spi_bus_remove_device(spi_read_device);
    }
    if (!config->flags.skip_init_host) {
        spi_bus_free(config->qspi.host_id);
    }
    free(panel);

    return ret;
}

esp_err_t esp_lcd_st77903_qspi_register_event_callbacks(esp_lcd_panel_t *handle, const st77903_qspi_event_callbacks_t *callbacks, void *user_ctx)
{
    ESP_RETURN_ON_FALSE(handle && callbacks, ESP_ERR_INVALID_ARG, TAG, "invalid argument");
    st77903_qspi_panel_t *panel = __containerof(handle, st77903_qspi_panel_t, base);
#if CONFIG_LCD_ST77903_ISR_IRAM_SAFE
    if (callbacks->on_vsync) {
        ESP_RETURN_ON_FALSE(esp_ptr_in_iram(callbacks->on_vsync), ESP_ERR_INVALID_ARG, TAG, "on_vsync callback not in IRAM");
    }
    if (callbacks->on_bounce_frame_finish) {
        ESP_RETURN_ON_FALSE(esp_ptr_in_iram(callbacks->on_bounce_frame_finish), ESP_ERR_INVALID_ARG, TAG, "on_bounce_frame_finish callback not in IRAM");
    }
    if (user_ctx) {
        ESP_RETURN_ON_FALSE(esp_ptr_in_iram(user_ctx), ESP_ERR_INVALID_ARG, TAG, "user_ctx not in IRAM");
    }
#endif
    panel->on_vsync = callbacks->on_vsync;
    panel->on_bounce_frame_finish = callbacks->on_bounce_frame_finish;
    panel->user_ctx = user_ctx;
    return ESP_OK;
}

esp_err_t esp_lcd_st77903_qspi_get_fps(esp_lcd_panel_handle_t handle, uint8_t *fps)
{
    st77903_qspi_panel_t *panel = __containerof(handle, st77903_qspi_panel_t, base);
    ESP_RETURN_ON_FALSE(panel && fps, ESP_ERR_INVALID_ARG, TAG, "invalid argument");
    ESP_RETURN_ON_FALSE(panel->flags.enable_cal_fps, ESP_ERR_INVALID_STATE, TAG, "FPS calculation is disabled, please enable it first");

    *fps = panel->fps;
    return ESP_OK;
}

static esp_err_t panel_st77903_qspi_init(esp_lcd_panel_t *handle)
{
    st77903_qspi_panel_t *panel = __containerof(handle, st77903_qspi_panel_t, base);
    if (panel->task.refresh_task_handle != NULL) {
        return ESP_OK;
    }

    if (panel->flags.fb_in_psram) {
        // Because data in PSRAM can't be transmitted by SPI DMA currently,
        // so we have to copy them from frame buffer (PSRAM) to bounce buffer (SRAM) first.
        // Load memory task is used to copy data from frame buffer to bounce buffer
        panel->flags.stop_load_task = 0;
        xTaskCreatePinnedToCore(
            load_memory_task, "lcd_load_mem", panel->task.load_size, panel, panel->task.load_priority,
            &panel->task.load_task_handle, panel->task.load_core
        );
    }

    // Configure the LCD using initial commands
    if (panel->flags.panel_need_reconfig) {
        panel->flags.panel_need_reconfig = 0;
        ESP_RETURN_ON_ERROR(lcd_cmd_config(panel), TAG, "LCD cmd config failed");
        ESP_LOGD(TAG, "LCD cmd config done");
    }

    // Here, we should load transaction pool in advance to meet fast SPI timing
    // Then the rest loading operations will be finished by `post_trans_color_cb()` automatically
    panel->load_bb_index = 0;
    panel->load_line_cnt = 0;
    for (int i = 0; i < panel->trans_pool_num; i++) {
        load_trans_pool(panel, panel->trans_pool[i]);
    }
    // Refresh task is used to transmit the segments through SPI
    panel->write_pool_index = 0;
    panel->flags.stop_refresh_task = 0;
    xTaskCreatePinnedToCore(
        refresh_task, "lcd_refresh", panel->task.refresh_size, panel, panel->task.refresh_priority,
        &panel->task.refresh_task_handle, panel->task.refresh_core
    );

    return ESP_OK;
}

static esp_err_t panel_st77903_qspi_reset(esp_lcd_panel_t *handle)
{
    st77903_qspi_panel_t *panel = __containerof(handle, st77903_qspi_panel_t, base);

    // Stop LCD refresh related tasks
    ESP_RETURN_ON_ERROR(stop_refresh(panel), TAG, "Stop refresh failed");

    // Perform hardware reset
    if (panel->reset_gpio_num >= 0) {
        gpio_set_level(panel->reset_gpio_num, !panel->flags.reset_level);
        vTaskDelay(pdMS_TO_TICKS(10));
        gpio_set_level(panel->reset_gpio_num, panel->flags.reset_level);
        vTaskDelay(pdMS_TO_TICKS(10));
        gpio_set_level(panel->reset_gpio_num, !panel->flags.reset_level);
        vTaskDelay(pdMS_TO_TICKS(120));
    } else {
        // Perform software reset
        ESP_RETURN_ON_ERROR(lcd_write_cmd(panel, LCD_CMD_SWRESET, NULL, 0), TAG, "LCD write cmd failed");
        vTaskDelay(pdMS_TO_TICKS(120));
    }
    panel->flags.panel_need_reconfig = 1;

    return ESP_OK;
}

static esp_err_t panel_st77903_qspi_del(esp_lcd_panel_t *handle)
{
    st77903_qspi_panel_t *panel = __containerof(handle, st77903_qspi_panel_t, base);

    // Stop LCD refresh related tasks
    ESP_RETURN_ON_ERROR(stop_refresh(panel), TAG, "Stop refresh failed");

    // Reset gpio
    if (panel->reset_gpio_num >= 0) {
        gpio_reset_pin(panel->reset_gpio_num);
    }

    // Reset SPI
    ESP_RETURN_ON_ERROR(spi_bus_remove_device(panel->spi_write_dev), TAG, "SPI write device remove failed");
    if (panel->flags.enable_read_reg) {
        ESP_RETURN_ON_ERROR(spi_bus_remove_device(panel->spi_read_dev), TAG, "SPI read device remove failed");
    }
    if (!panel->flags.skip_init_host) {
        ESP_RETURN_ON_ERROR(spi_bus_free(panel->spi_host_id), TAG, "SPI bus free failed");
    }

    // Free others
    if (panel->flags.enable_read_reg) {
        vSemaphoreDelete(panel->sem_read_ready);
        vSemaphoreDelete(panel->sem_read_done);
    }

    // Free buffers' memory
    for (int i = 0; i < panel->trans_pool_num; i++) {
        free(panel->trans_pool[i]);
    }
    for (int i = 0; i < panel->fb_num; i++) {
        free(panel->fbs[i]);
    }
    free(panel->fbs);
    vSemaphoreDelete(panel->sem_count_free_trans);
    if (panel->flags.fb_in_psram) {
        vQueueDelete(panel->queue_load_mem_info);
        for (int i = 0; i < panel->trans_pool_num; i++) {
            free(panel->bbs[i]);
        }
        free(panel->bbs);
    }

    ESP_LOGD(TAG, "Del panel @%p", panel);
    free(panel);

    return ESP_OK;
}

__attribute__((always_inline))
static inline void copy_pixel_16bpp(uint8_t *to, const uint8_t *from)
{
    *to++ = *from++;
    *to++ = *from++;
}

__attribute__((always_inline))
static inline void copy_pixel_24bpp(uint8_t *to, const uint8_t *from)
{
    *to++ = *from++;
    *to++ = *from++;
    *to++ = *from++;
}

#define COPY_PIXEL_CODE_BLOCK(_bpp)                                                               \
    switch (panel->rotate_mask)                                                               \
    {                                                                                             \
    case 0:                                                                                       \
    {                                                                                             \
        uint8_t *to = fb + (y_start * h_res + x_start) * bytes_per_pixel;                         \
        for (int y = y_start; y < y_end; y++)                                                     \
        {                                                                                         \
            memcpy(to, from, copy_bytes_per_line);                                                \
            to += bytes_per_line;                                                                 \
            from += copy_bytes_per_line;                                                          \
        }                                                                                         \
    }                                                                                             \
    break;                                                                                        \
    case ROTATE_MASK_MIRROR_X:                                                                    \
        for (int y = y_start; y < y_end; y++)                                                     \
        {                                                                                         \
            uint32_t index = (y * h_res + (h_res - 1 - x_start)) * bytes_per_pixel;               \
            for (size_t x = x_start; x < x_end; x++)                                              \
            {                                                                                     \
                copy_pixel_##_bpp##bpp(to + index, from);                                         \
                index -= bytes_per_pixel;                                                         \
                from += bytes_per_pixel;                                                          \
            }                                                                                     \
        }                                                                                         \
        break;                                                                                    \
    case ROTATE_MASK_MIRROR_Y:                                                                    \
    {                                                                                             \
        uint8_t *to = fb + ((v_res - 1 - y_start) * h_res + x_start) * bytes_per_pixel;           \
        for (int y = y_start; y < y_end; y++)                                                     \
        {                                                                                         \
            memcpy(to, from, copy_bytes_per_line);                                                \
            to -= bytes_per_line;                                                                 \
            from += copy_bytes_per_line;                                                          \
        }                                                                                         \
    }                                                                                             \
    break;                                                                                        \
    case ROTATE_MASK_MIRROR_X | ROTATE_MASK_MIRROR_Y:                                             \
        for (int y = y_start; y < y_end; y++)                                                     \
        {                                                                                         \
            uint32_t index = ((v_res - 1 - y) * h_res + (h_res - 1 - x_start)) * bytes_per_pixel; \
            for (size_t x = x_start; x < x_end; x++)                                              \
            {                                                                                     \
                copy_pixel_##_bpp##bpp(to + index, from);                                         \
                index -= bytes_per_pixel;                                                         \
                from += bytes_per_pixel;                                                          \
            }                                                                                     \
        }                                                                                         \
        break;                                                                                    \
    case ROTATE_MASK_SWAP_XY:                                                                     \
        for (int y = y_start; y < y_end; y++)                                                     \
        {                                                                                         \
            for (int x = x_start; x < x_end; x++)                                                 \
            {                                                                                     \
                uint32_t j = y * copy_bytes_per_line + x * bytes_per_pixel - offset;              \
                uint32_t i = (x * h_res + y) * bytes_per_pixel;                                   \
                copy_pixel_##_bpp##bpp(to + i, from + j);                                         \
            }                                                                                     \
        }                                                                                         \
        break;                                                                                    \
    case ROTATE_MASK_SWAP_XY | ROTATE_MASK_MIRROR_X:                                              \
        for (int y = y_start; y < y_end; y++)                                                     \
        {                                                                                         \
            for (int x = x_start; x < x_end; x++)                                                 \
            {                                                                                     \
                uint32_t j = y * copy_bytes_per_line + x * bytes_per_pixel - offset;              \
                uint32_t i = (x * h_res + h_res - 1 - y) * bytes_per_pixel;                       \
                copy_pixel_##_bpp##bpp(to + i, from + j);                                         \
            }                                                                                     \
        }                                                                                         \
        break;                                                                                    \
    case ROTATE_MASK_SWAP_XY | ROTATE_MASK_MIRROR_Y:                                              \
        for (int y = y_start; y < y_end; y++)                                                     \
        {                                                                                         \
            for (int x = x_start; x < x_end; x++)                                                 \
            {                                                                                     \
                uint32_t j = y * copy_bytes_per_line + x * bytes_per_pixel - offset;              \
                uint32_t i = ((v_res - 1 - x) * h_res + y) * bytes_per_pixel;                     \
                copy_pixel_##_bpp##bpp(to + i, from + j);                                         \
            }                                                                                     \
        }                                                                                         \
        break;                                                                                    \
    case ROTATE_MASK_SWAP_XY | ROTATE_MASK_MIRROR_X | ROTATE_MASK_MIRROR_Y:                       \
        for (int y = y_start; y < y_end; y++)                                                     \
        {                                                                                         \
            for (int x = x_start; x < x_end; x++)                                                 \
            {                                                                                     \
                uint32_t j = y * copy_bytes_per_line + x * bytes_per_pixel - offset;              \
                uint32_t i = ((v_res - 1 - x) * h_res + h_res - 1 - y) * bytes_per_pixel;         \
                copy_pixel_##_bpp##bpp(to + i, from + j);                                         \
            }                                                                                     \
        }                                                                                         \
        break;                                                                                    \
    default:                                                                                      \
        break;                                                                                    \
    }

static esp_err_t panel_st77903_qspi_draw_bitmap(esp_lcd_panel_t *handle, int x_start, int y_start, int x_end, int y_end, const void *color_data)
{
    st77903_qspi_panel_t *panel = __containerof(handle, st77903_qspi_panel_t, base);

    ESP_RETURN_ON_FALSE((x_start >= 0) && (y_start >= 0), ESP_ERR_INVALID_ARG, TAG, "start position must be bigger than zero point");
    ESP_RETURN_ON_FALSE((x_start < x_end) && (y_start < y_end), ESP_ERR_INVALID_ARG, TAG, "start position must be smaller than end position");
    ESP_RETURN_ON_FALSE(color_data, ESP_ERR_INVALID_ARG, TAG, "Invalid color_data");

    void **frame_buf = panel->fbs;
    bool do_copy = true;
    for (int i = 0; i < panel->fb_num; i++) {
        if (frame_buf[i] == color_data) {
            panel->next_fb_index = i;
            do_copy = false;
            break;
        }
    }

    // adjust the flush window by adding extra gap
    x_start += panel->x_gap;
    y_start += panel->y_gap;
    x_end += panel->x_gap;
    y_end += panel->y_gap;
    // round the boundary
    int h_res = panel->hor_res;
    int v_res = panel->ver_res;
    if (panel->rotate_mask & ROTATE_MASK_SWAP_XY) {
        x_start = MIN(x_start, v_res);
        x_end = MIN(x_end, v_res);
        y_start = MIN(y_start, h_res);
        y_end = MIN(y_end, h_res);
    } else {
        x_start = MIN(x_start, h_res);
        x_end = MIN(x_end, h_res);
        y_start = MIN(y_start, v_res);
        y_end = MIN(y_end, v_res);
    }

    int bytes_per_pixel = panel->bytes_per_pixel;
    int pixels_per_line = panel->hor_res;
    uint32_t bytes_per_line = bytes_per_pixel * pixels_per_line;
    uint8_t *fb = frame_buf[panel->cur_fb_index];

    if (do_copy) {
        // copy the UI draw buffer into internal frame buffer
        const uint8_t *from = (const uint8_t *)color_data;
        uint32_t copy_bytes_per_line = (x_end - x_start) * bytes_per_pixel;
        size_t offset = y_start * copy_bytes_per_line + x_start * bytes_per_pixel;
        uint8_t *to = fb;
        if (2 == bytes_per_pixel) {
            COPY_PIXEL_CODE_BLOCK(16)
        } else if (3 == bytes_per_pixel) {
            COPY_PIXEL_CODE_BLOCK(24)
        }
    }

    return ESP_OK;
}

static esp_err_t panel_st77903_qspi_mirror(esp_lcd_panel_t *handle, bool mirror_x, bool mirror_y)
{
    st77903_qspi_panel_t *panel = __containerof(handle, st77903_qspi_panel_t, base);

    if (panel->flags.mirror_by_cmd) {
        ESP_RETURN_ON_FALSE(panel->task.refresh_task_handle == NULL, ESP_ERR_INVALID_STATE, TAG,
                            "Mirror by command is not supported when refresh task is running, "
                            "please call `esp_lcd_panel_disp_on_off()` to stop refresh task first");
        // Control mirror through LCD command
        if (mirror_x) {
            panel->madctl_val |= LCD_CMD_MH_BIT;
        } else {
            panel->madctl_val &= ~LCD_CMD_MH_BIT;
        }
        if (mirror_y) {
            panel->madctl_val |= LCD_CMD_ML_BIT;
        } else {
            panel->madctl_val &= ~LCD_CMD_ML_BIT;
        }
        ESP_RETURN_ON_ERROR(lcd_write_cmd(panel, LCD_CMD_MADCTL, &panel->madctl_val, 1), TAG, "LCD write cmd failed");
    } else {
        // Control mirror through software
        panel->rotate_mask &= ~(ROTATE_MASK_MIRROR_X | ROTATE_MASK_MIRROR_Y);
        panel->rotate_mask |= (mirror_x << PANEL_MIRROR_X | mirror_y << PANEL_MIRROR_Y);
    }
    return ESP_OK;
}

static esp_err_t panel_st77903_qspi_swap_xy(esp_lcd_panel_t *handle, bool swap_axes)
{
    st77903_qspi_panel_t *panel = __containerof(handle, st77903_qspi_panel_t, base);
    panel->rotate_mask &= ~(ROTATE_MASK_SWAP_XY);
    panel->rotate_mask |= swap_axes << PANEL_SWAP_XY;
    return ESP_OK;
}

static esp_err_t panel_st77903_qspi_set_gap(esp_lcd_panel_t *handle, int x_gap, int y_gap)
{
    ESP_RETURN_ON_FALSE(x_gap >= 0 && y_gap >= 0, ESP_ERR_INVALID_ARG, TAG, "x_gap and y_gap should not be less than 0");
    st77903_qspi_panel_t *panel = __containerof(handle, st77903_qspi_panel_t, base);
    panel->x_gap = x_gap;
    panel->y_gap = y_gap;

    return ESP_OK;
}

static esp_err_t stop_refresh(st77903_qspi_panel_t *panel)
{
    if (panel->task.refresh_task_handle == NULL) {
        return ESP_OK;
    }

    int64_t start_time = esp_timer_get_time();
    int64_t end_time = TASK_STOP_TIME_MAX_MS * 1000 + start_time;

    // Stop refrehs & load mem task
    panel->flags.stop_refresh_task = 1;
    while ((panel->task.refresh_task_handle != NULL) && (esp_timer_get_time() < end_time)) {
        vTaskDelay(pdTICKS_TO_MS(TASK_CHECK_TIME_MS));
    }
    while ((panel->task.load_task_handle != NULL) && (esp_timer_get_time() < end_time)) {
        vTaskDelay(pdTICKS_TO_MS(TASK_CHECK_TIME_MS));
    }
    ESP_RETURN_ON_FALSE(esp_timer_get_time() < end_time, ESP_ERR_TIMEOUT, TAG, "Stop refresh task timeout, "
                        "please increase task priority or `TASK_STOP_TIME_MAX_MS`");

    vTaskDelay(pdTICKS_TO_MS(TASK_STOP_WAIT_TIME_MS));
    if (panel->queue_load_mem_info) {
        xQueueReset(panel->queue_load_mem_info);
    }
    for (int i = 0; i < panel->trans_pool_size; i++) {
        xSemaphoreTake(panel->sem_count_free_trans, 0);
    }

    return ESP_OK;
}

static esp_err_t panel_st77903_qspi_disp_on_off(esp_lcd_panel_t *handle, bool on_off)
{
    st77903_qspi_panel_t *panel = __containerof(handle, st77903_qspi_panel_t, base);

    if (!on_off) {
        // Stop refresh task before sending the command
        ESP_RETURN_ON_ERROR(stop_refresh(panel), TAG, "Stop refresh failed");
        // Send command to turn off display
        ESP_RETURN_ON_ERROR(lcd_write_cmd(panel, LCD_CMD_DISPOFF, NULL, 0), TAG, "send command failed");
    } else {
        ESP_RETURN_ON_FALSE(panel->task.refresh_task_handle == NULL, ESP_ERR_INVALID_STATE, TAG,
                            "Cannot set display on when refresh task is running");
        // Send command to turn on display
        ESP_RETURN_ON_ERROR(lcd_write_cmd(panel, LCD_CMD_DISPON, NULL, 0), TAG, "send command failed");
        // Start refresh task if it is not running
        ESP_RETURN_ON_ERROR(panel_st77903_qspi_init(handle), TAG, "Start refresh failed");
    }

    return ESP_OK;
}

esp_err_t esp_lcd_st77903_qspi_get_frame_buffer(esp_lcd_panel_handle_t handle, uint32_t fb_num, void **fb0, ...)
{
    ESP_RETURN_ON_FALSE(handle, ESP_ERR_INVALID_ARG, TAG, "Invalid handle");

    st77903_qspi_panel_t *panel = __containerof(handle, st77903_qspi_panel_t, base);
    ESP_RETURN_ON_FALSE(fb_num && fb_num <= panel->fb_num, ESP_ERR_INVALID_ARG, TAG, "Frame buffer num out of range(< %d)", panel->fb_num);

    void **fb_itor = fb0;
    va_list args;
    va_start(args, fb0);
    for (int i = 0; i < fb_num; i++) {
        if (fb_itor) {
            *fb_itor = panel->fbs[i];
            fb_itor = va_arg(args, void **);
        }
    }
    va_end(args);
    return ESP_OK;
}

esp_err_t esp_lcd_st77903_qspi_read_reg(esp_lcd_panel_handle_t handle, uint8_t reg, uint8_t *data, size_t data_size, TickType_t timeout)
{
    ESP_RETURN_ON_FALSE(handle, ESP_ERR_INVALID_ARG, TAG, "Invalid handle");
    ESP_RETURN_ON_FALSE(data && data_size, ESP_ERR_INVALID_ARG, TAG, "Invalid data");
    st77903_qspi_panel_t *panel = __containerof(handle, st77903_qspi_panel_t, base);
    ESP_RETURN_ON_FALSE(panel->flags.enable_read_reg, ESP_ERR_NOT_SUPPORTED, TAG, "Read reg is not enabled");

    panel->read_reg = reg;
    panel->read_data = data;
    panel->read_data_size = data_size;
    panel->flags.wait_frame_end = true;
    if (xSemaphoreTake(panel->sem_read_done, timeout) != pdTRUE) {
        return ESP_ERR_TIMEOUT;
    }

    return ESP_OK;
}

IRAM_ATTR static bool load_trans_pool(st77903_qspi_panel_t *panel, spi_multi_transaction_t *seg_trans)
{
    uint32_t cur_bb_fb_index = panel->cur_bb_fb_index;
    uint32_t cur_fb_index = panel->cur_fb_index;
    uint32_t load_line_cnt = panel->load_line_cnt;
    uint16_t trans_num = MIN(panel->trans_pool_size, panel->ver_res - load_line_cnt);
    uint8_t *load_buf;
    uint8_t *next_load_buf;
    uint16_t next_trans_num;
    void **fbs = panel->fbs;
    void **bbs = panel->bbs;
    BaseType_t need_yield = pdFALSE;
    bool in_isr = (xPortInIsrContext() == pdTRUE);

    panel->load_line_cnt += trans_num;
    //  We should switch to next frame buffer after the last line of the frame is loaded
    if (panel->load_line_cnt >= panel->ver_res) {
        panel->load_line_cnt = 0;
        if (panel->flags.fb_in_psram) {
            panel->cur_bb_fb_index = panel->next_fb_index;
        }
        panel->cur_fb_index = panel->next_fb_index;
        if (panel->on_bounce_frame_finish) {
            if (panel->on_bounce_frame_finish(&panel->base, NULL, panel->user_ctx)) {
                need_yield = pdTRUE;
            }
        }
    }

    if (panel->flags.fb_in_psram) {
        // If frame buffers are in PSRAM, they can't be transmitted by SPI DMA currently, so we have to use bounce buffers to transmit them
        // Here, we assemble the addresses to queue and use load memory task to copy data from frame buffer to bounce buffer
        load_buf = (uint8_t *)fbs[cur_bb_fb_index] + load_line_cnt * panel->bytes_per_line;
        next_load_buf = (uint8_t *)fbs[panel->cur_bb_fb_index] + panel->load_line_cnt * panel->bytes_per_line;
        next_trans_num = MIN(panel->trans_pool_size, panel->ver_res - panel->load_line_cnt);
        queue_load_mem_info_t load_info = {
            .from = (void *)load_buf,
            .to = (void *)bbs[panel->load_bb_index],
            .bytes = trans_num * panel->bytes_per_line,
            .next_buf = (void *)next_load_buf,
            .next_buf_bytes = next_trans_num * panel->bytes_per_line,
        };
        if (in_isr) {
            xQueueSendFromISR(panel->queue_load_mem_info, &load_info, &need_yield);
        } else {
            xQueueSend(panel->queue_load_mem_info, &load_info, portMAX_DELAY);
        }
        load_buf = (uint8_t *)(bbs[panel->load_bb_index]);
        panel->load_bb_index++;
        if (panel->load_bb_index >= panel->trans_pool_num) {
            panel->load_bb_index = 0;
        }
    } else {
        // If frame buffers are in SRAM, they can be transmitted by SPI DMA directly
        // So, we should release semaphore to allow refresh task continue
        load_buf = (uint8_t *)fbs[cur_fb_index] + load_line_cnt * panel->bytes_per_line;
        if (in_isr) {
            xSemaphoreGiveFromISR(panel->sem_count_free_trans, &need_yield);
        } else {
            xSemaphoreGive(panel->sem_count_free_trans);
        }
    }

    for (int i = 0; i < trans_num; i++) {
        seg_trans[i].base.tx_buffer = (void *)load_buf;
        load_buf += panel->bytes_per_line;
    }

    return (need_yield == pdTRUE);
}

IRAM_ATTR static void post_trans_color_cb(spi_transaction_t *trans)
{
    BaseType_t need_yield = pdFALSE;
    custom_user_data_t *user = (custom_user_data_t *)trans->user;

    if (user != NULL) {
        st77903_qspi_panel_t *panel = user->panel;
        if (user->is_color) {  // Trans color end
            if (load_trans_pool(panel, (spi_multi_transaction_t *)trans)) {
                need_yield = pdTRUE;
            }
        } else if (panel->flags.enable_read_reg && user->is_vfp && panel->flags.wait_frame_end) { // Trans VFP end
            xSemaphoreGiveFromISR(panel->sem_read_ready, &need_yield);
        } else if (!user->is_vfp) { // Trans VBP end
            esp_rom_delay_us(LCD_LINE_INTERVAL_MIN_US);
            if (panel->on_vsync) {
                if (panel->on_vsync(&panel->base, NULL, panel->user_ctx)) {
                    need_yield = pdTRUE;
                }
            }
        }
    }

    if (need_yield == pdTRUE) {
        portYIELD_FROM_ISR();
    }
}

static esp_err_t lcd_write_cmd(st77903_qspi_panel_t *panel, uint8_t cmd, const void *param, size_t param_size)
{
    spi_multi_transaction_t *send_seg = &panel->write_cmd_seg;
    memset(send_seg, 0, sizeof(spi_multi_transaction_t));

    send_seg->base.cmd = ST77903_INS_CMD;
    send_seg->base.user = NULL;
    send_seg->base.addr = ((uint32_t)cmd) << 8;
    send_seg->base.length = param_size * 8;
    send_seg->base.tx_buffer = (param_size == 0) ? NULL : (void *)param;
    ESP_RETURN_ON_ERROR(spi_device_queue_multi_trans(panel->spi_write_dev, send_seg, 1, portMAX_DELAY), TAG, "SPI segment trans failed");
    esp_rom_delay_us(LCD_LINE_INTERVAL_MIN_US);

    return ESP_OK;
}

static esp_err_t lcd_write_color(st77903_qspi_panel_t *panel)
{
    int row_count = panel->ver_res;
    int pool_size = 0;

    do {
        pool_size = MIN(row_count, panel->trans_pool_size);
        xSemaphoreTake(panel->sem_count_free_trans, portMAX_DELAY);
        ESP_RETURN_ON_ERROR(spi_device_queue_multi_trans(panel->spi_write_dev, panel->trans_pool[panel->write_pool_index], pool_size,
                                                         portMAX_DELAY), TAG, "SPI segment trans failed");
        panel->write_pool_index++;
        if (panel->write_pool_index >= panel->trans_pool_num) {
            panel->write_pool_index = 0;
        }
        row_count -= pool_size;
    } while (row_count > 0);

    return ESP_OK;
}

static esp_err_t lcd_read_reg(st77903_qspi_panel_t *panel)
{
    ESP_RETURN_ON_FALSE(panel->flags.enable_read_reg, ESP_ERR_INVALID_STATE, TAG, "Read reg not enabled");

    spicommon_cs_initialize(panel->spi_host_id, panel->cs_io_num, get_spi_device_id(panel->spi_read_dev), 1);
    ESP_RETURN_ON_ERROR(spi_bus_multi_trans_mode_enable(panel->spi_write_dev, false), TAG, "Segment mode disenable failed");

    spi_transaction_t send_seg = {
        .cmd = ST77903_INS_READ,
        .addr = ((uint32_t)panel->read_reg) << 8,
                                            .rxlength = 8 * panel->read_data_size,
                                            .tx_buffer = NULL,
                                            .rx_buffer = panel->read_data,
    };
    ESP_RETURN_ON_ERROR(spi_device_polling_transmit(panel->spi_read_dev, &send_seg), TAG, "SPI polling trans failed");
    ESP_RETURN_ON_ERROR(spi_bus_multi_trans_mode_enable(panel->spi_write_dev, true), TAG, "Segment mode enable failed");
    spicommon_cs_initialize(panel->spi_host_id, panel->cs_io_num, get_spi_device_id(panel->spi_write_dev), 1);

    return ESP_OK;
}

static esp_err_t lcd_write_vsync(st77903_qspi_panel_t *panel, bool is_front)
{
    if (is_front) {
        ESP_RETURN_ON_ERROR(spi_device_queue_multi_trans(panel->spi_write_dev, panel->vsync_front_pool, LCD_VSYNC_FRONT_NUM, portMAX_DELAY),
                            TAG, "spi seg trans failed");
    } else {
        ESP_RETURN_ON_ERROR(spi_device_queue_multi_trans(panel->spi_write_dev, panel->vsync_back_pool, LCD_VSYNC_BACK_NUM + 1, portMAX_DELAY),
                            TAG, "spi seg trans failed");
    }

    return ESP_OK;
}

static void load_memory_task(void *arg)
{
    st77903_qspi_panel_t *panel = (st77903_qspi_panel_t *)arg;
    BaseType_t result = pdFALSE;
    queue_load_mem_info_t load_info;

    ESP_LOGD(TAG, "Load memory task start");
    while (1) {
        result = pdFALSE;
        while (result == pdFALSE) {
            result = xQueueReceive(panel->queue_load_mem_info, &load_info, pdMS_TO_TICKS(TASK_CHECK_TIME_MS));
            if (panel->flags.stop_load_task) {
                panel->flags.stop_load_task = false;
                goto end;
            }
        }
        memcpy(load_info.to, load_info.from, load_info.bytes);
#if CONFIG_IDF_TARGET_ESP32S3
        // Preload the next bounce buffer from psram
        Cache_Start_DCache_Preload((uint32_t)load_info.next_buf, load_info.next_buf_bytes, 0);
#endif
        xSemaphoreGive(panel->sem_count_free_trans);
    }

end:
    panel->task.load_task_handle = NULL;
    ESP_LOGD(TAG, "Load memory task stop");
    vTaskDelete(NULL);
}

static void refresh_task(void *arg)
{
    st77903_qspi_panel_t *panel = (st77903_qspi_panel_t *)arg;
    uint16_t frame_cnt = 0;
    int64_t start_time = 0;
    esp_err_t ret = ESP_OK;

    ESP_LOGD(TAG, "Refresh task start");
    if (panel->flags.enable_cal_fps) {
        start_time  = esp_timer_get_time();
    }

    while (!panel->flags.stop_refresh_task) {
        // Send vsync front command
        ESP_GOTO_ON_ERROR(lcd_write_vsync(panel, 0), end, TAG, "LCD write vsync front cmd failed");
        // Send the whole frame data
        ESP_GOTO_ON_ERROR(lcd_write_color(panel), end, TAG, "LCD write color failed");
        // Send vsync back commands
        ESP_GOTO_ON_ERROR(lcd_write_vsync(panel, 1), end, TAG, "LCD write vsync back cmd failed");

        // Read LCD register
        if (panel->flags.enable_read_reg && panel->flags.wait_frame_end) {
            xSemaphoreTake(panel->sem_read_ready, 0);
            // Wait for the frame end
            if (xSemaphoreTake(panel->sem_read_ready, pdMS_TO_TICKS(READ_WAIT_TIME_MAX_MS)) == pdTRUE) {
                panel->flags.wait_frame_end = false;
                ESP_GOTO_ON_ERROR(lcd_read_reg(panel), end, TAG, "LCD read reg failed");
                xSemaphoreGive(panel->sem_read_done);
            }
        }

        // Calculate FPS
        if (panel->flags.enable_cal_fps) {
            if (++frame_cnt == 100) {
                frame_cnt = 0;
                panel->fps = (int)(100000000ULL / (esp_timer_get_time() - start_time));
                start_time = esp_timer_get_time();
            }
        }
    }

end:
    panel->flags.stop_load_task = true;
    panel->task.refresh_task_handle = NULL;
    ESP_LOGD(TAG, "Refresh task stop (%s)", esp_err_to_name(ret));
    vTaskDelete(NULL);
}

static const st77903_lcd_init_cmd_t vendor_specific_init_default[] = {
//  {cmd, { data }, data_size, delay_ms}
    {0xf0, (uint8_t []){0xc3}, 1, 0},
    {0xf0, (uint8_t []){0x96}, 1, 0},
    {0xf0, (uint8_t []){0xa5}, 1, 0},
    {0xe9, (uint8_t []){0x20}, 1, 0},
    {0xe7, (uint8_t []){0x80, 0x77, 0x1f, 0xcc}, 4, 0},
    {0xc1, (uint8_t []){0x77, 0x07, 0xcf, 0x16}, 4, 0},
    {0xc2, (uint8_t []){0x77, 0x07, 0xcf, 0x16}, 4, 0},
    {0xc3, (uint8_t []){0x22, 0x02, 0x22, 0x04}, 4, 0},
    {0xc4, (uint8_t []){0x22, 0x02, 0x22, 0x04}, 4, 0},
    {0xc5, (uint8_t []){0xed}, 1, 0},
    {0xe0, (uint8_t []){0x87, 0x09, 0x0c, 0x06, 0x05, 0x03, 0x29, 0x32, 0x49, 0x0f, 0x1b, 0x17, 0x2a, 0x2f}, 14, 0},
    {0xe1, (uint8_t []){0x87, 0x09, 0x0c, 0x06, 0x05, 0x03, 0x29, 0x32, 0x49, 0x0f, 0x1b, 0x17, 0x2a, 0x2f}, 14, 0},
    {0xe5, (uint8_t []){0xbe, 0xf5, 0xb1, 0x22, 0x22, 0x25, 0x10, 0x22, 0x22, 0x22, 0x22, 0x22, 0x22, 0x22}, 14, 0},
    {0xe6, (uint8_t []){0xbe, 0xf5, 0xb1, 0x22, 0x22, 0x25, 0x10, 0x22, 0x22, 0x22, 0x22, 0x22, 0x22, 0x22}, 14, 0},
    {0xec, (uint8_t []){0x40, 0x03}, 2, 0},
    {0xb2, (uint8_t []){0x00}, 1, 0},
    {0xb3, (uint8_t []){0x01}, 1, 0},
    {0xb4, (uint8_t []){0x00}, 1, 0},
    {0xa5, (uint8_t []){0x00, 0x00, 0x00, 0x00, 0x00, 0x15, 0x2a, 0x8a, 0x02}, 9, 0},
    {0xa6, (uint8_t []){0x00, 0x00, 0x00, 0x00, 0x00, 0x15, 0x2a, 0x8a, 0x02}, 9, 0},
    {0xba, (uint8_t []){0x0a, 0x5a, 0x23, 0x10, 0x25, 0x02, 0x00}, 7, 0},
    {0xbb, (uint8_t []){0x00, 0x30, 0x00, 0x2c, 0x82, 0x87, 0x18, 0x00}, 8, 0},
    {0xbc, (uint8_t []){0x00, 0x30, 0x00, 0x2c, 0x82, 0x87, 0x18, 0x00}, 8, 0},
    {0xbd, (uint8_t []){0xa1, 0xb2, 0x2b, 0x1a, 0x56, 0x43, 0x34, 0x65, 0xff, 0xff, 0x0f}, 11, 0},
    {0x35, (uint8_t []){0x00}, 1, 0},
    {0x21, (uint8_t []){0x00}, 0, 0},
    {0x11, (uint8_t []){0x00}, 0, 120},

    // {0xb0, (uint8_t []){0xa5}, 1, 0},               /* This part of the parameters can be used for screen self-test */
    // {0xcc, (uint8_t []){0x40, 0x00, 0x3f, 0x00, 0x14, 0x14, 0x20, 0x20, 0x03}, 9, 0},
};

static esp_err_t lcd_cmd_config(st77903_qspi_panel_t *panel)
{
    ESP_RETURN_ON_ERROR(lcd_write_cmd(panel, 0xf0, (uint8_t []) {
        0xc3
    }, 1), TAG, "Write cmd failed");
    ESP_RETURN_ON_ERROR(lcd_write_cmd(panel, 0xf0, (uint8_t []) {
        0x96
    }, 1), TAG, "Write cmd failed");
    ESP_RETURN_ON_ERROR(lcd_write_cmd(panel, 0xf0, (uint8_t []) {
        0xa5
    }, 1), TAG, "Write cmd failed");
    uint8_t NL = (panel->ver_res >> 1) - 1;
    uint8_t NC = (panel->hor_res >> 3) - 1;
    // Set Resolution
    ESP_RETURN_ON_ERROR(lcd_write_cmd(panel, ST77903_CMD_DISCN, (uint8_t []) {
        NL, NC
    }, 2), TAG, "Write cmd failed");
    // Set VFP & VBP
    ESP_RETURN_ON_ERROR(lcd_write_cmd(panel, ST77903_CMD_BPC, (uint8_t []) {
        0x00, LCD_VSYNC_FRONT_NUM, 0x00, LCD_VSYNC_BACK_NUM
    }, 4), TAG, "Write cmd failed");
    // Set color format
    ESP_RETURN_ON_ERROR(lcd_write_cmd(panel, LCD_CMD_MADCTL, (uint8_t []) {
        panel->madctl_val
    }, 1), TAG, "Write cmd failed");
    ESP_RETURN_ON_ERROR(lcd_write_cmd(panel, LCD_CMD_COLMOD, (uint8_t []) {
        panel->colmod_val
    }, 1), TAG, "Write cmd failed");

    // vendor specific initialization, it can be different between manufacturers
    // should consult the LCD supplier for initialization sequence code
    const st77903_lcd_init_cmd_t *init_cmds = NULL;
    uint16_t init_cmds_size = 0;
    if (panel->init_cmds) {
        init_cmds = panel->init_cmds;
        init_cmds_size = panel->init_cmds_size;
    } else {
        init_cmds = vendor_specific_init_default;
        init_cmds_size = sizeof(vendor_specific_init_default) / sizeof(st77903_lcd_init_cmd_t);
    }

    bool is_cmd_overwritten = false;
    bool is_cmd_conflicting = false;
    for (int i = 0; i < init_cmds_size; i++) {
        // Check if the command has been used or conflicts with the internal
        switch (init_cmds[i].cmd) {
        case LCD_CMD_MADCTL:
            is_cmd_overwritten = true;
            panel->madctl_val = ((uint8_t *)init_cmds[i].data)[0];
            break;
        case LCD_CMD_COLMOD:
            is_cmd_overwritten = true;
            panel->colmod_val = ((uint8_t *)init_cmds[i].data)[0];
            break;
        case ST77903_CMD_DISCN:
            if ((init_cmds[i].data_bytes >= 2) && ((((uint8_t *)init_cmds[i].data)[0] != NL) ||
                                                   (((uint8_t *)init_cmds[i].data)[1] != NC))) {
                is_cmd_conflicting = true;
            }
            break;
        case ST77903_CMD_BPC:
            if ((init_cmds[i].data_bytes >= 4) && ((((uint8_t *)init_cmds[i].data)[1] != LCD_VSYNC_FRONT_NUM) ||
                                                   (((uint8_t *)init_cmds[i].data)[3] != LCD_VSYNC_BACK_NUM))) {
                is_cmd_conflicting = true;
            }
            break;
        default:
            is_cmd_overwritten = false;
            is_cmd_conflicting = false;
            break;
        }

        if (is_cmd_overwritten) {
            ESP_LOGW(TAG, "The %02Xh command has been used and will be overwritten by external initialization sequence",
                     init_cmds[i].cmd);
        } else if (is_cmd_conflicting) {
            ESP_LOGE(TAG, "The %02Xh command conflicts with the internal, please remove it from external initialization sequence",
                     init_cmds[i].cmd);
        }

        // Only send the command if it is not conflicted
        if (!is_cmd_conflicting) {
            ESP_RETURN_ON_ERROR(lcd_write_cmd(panel, init_cmds[i].cmd, init_cmds[i].data, init_cmds[i].data_bytes), TAG,
                                "Write cmd failed");
            vTaskDelay(pdMS_TO_TICKS(init_cmds[i].delay_ms));
        }
    }
    ESP_LOGD(TAG, "send init commands success");

    return ESP_OK;
}
