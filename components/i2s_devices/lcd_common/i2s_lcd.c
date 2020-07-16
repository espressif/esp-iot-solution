// Copyright 2015-2016 Espressif Systems (Shanghai) PTE LTD
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
#include <string.h>
#include <math.h>
#include <esp_types.h>

#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/xtensa_api.h"
#include "soc/dport_reg.h"
#include "esp32/rom/lldesc.h"
#include "driver/gpio.h"
#include "iot_i2s_lcd.h"
#include "esp_intr_alloc.h"
#include "esp_err.h"
#include "esp_log.h"

#define I2S_CHECK(a, str, ret) if (!(a)) {                                              \
        ESP_LOGE(I2S_TAG,"%s:%d (%s):%s", __FILE__, __LINE__, __FUNCTION__, str);       \
        return (ret);                                                                   \
        }
#define I2S_MAX_BUFFER_SIZE (4 * 1024 * 1024) //the maximum RAM can be allocated
#define I2S_BASE_CLK (2*APB_CLK_FREQ)
#define I2S_ENTER_CRITICAL_ISR()     portENTER_CRITICAL_ISR(&i2s_spinlock[i2s_num])
#define I2S_EXIT_CRITICAL_ISR()      portEXIT_CRITICAL_ISR(&i2s_spinlock[i2s_num])
#define I2S_ENTER_CRITICAL()         portENTER_CRITICAL(&i2s_spinlock[i2s_num])
#define I2S_EXIT_CRITICAL()          portEXIT_CRITICAL(&i2s_spinlock[i2s_num])

//This macro definition only for lcd and camera mode.
//In i2s dma link descriptor, the maximum of dma buffer is 4095 bytes,
//so we take the length of 1023 words here.
#define DMA_SIZE (2000)  //words
/**
 * @brief DMA buffer object
 *
 */
typedef struct {
    char **buf;
    int buf_size;
    int rw_pos;
    void *curr_ptr;
    SemaphoreHandle_t mux;
    xQueueHandle queue;
    lldesc_t **desc;
} i2s_dma_t;

/**
 * @brief I2S object instance
 *
 */
typedef struct {
    i2s_port_t i2s_num;              /*!< I2S port number*/
    int queue_size;                  /*!< I2S event queue size*/
    QueueHandle_t i2s_queue;         /*!< I2S queue handler*/
    int dma_buf_count;               /*!< DMA buffer count, number of buffer*/
    int dma_buf_len;                 /*!< DMA buffer length, length of each buffer*/
    i2s_dma_t *rx;                   /*!< DMA Tx buffer*/
    i2s_dma_t *tx;                   /*!< DMA Rx buffer*/
    i2s_isr_handle_t i2s_isr_handle; /*!< I2S Interrupt handle*/
    int channel_num;                 /*!< Number of channels*/
    int bytes_per_sample;            /*!< Bytes per sample*/
    int bits_per_sample;             /*!< Bits per sample*/
    i2s_mode_t mode;                 /*!< I2S Working mode*/
    uint32_t sample_rate;            /*!< I2S sample rate */
    bool use_apll;                   /*!< I2S use APLL clock */
    int fixed_mclk;                  /*!< I2S fixed MLCK clock */
} i2s_obj_t;

static const char *I2S_TAG = "IOT_I2S";
static i2s_obj_t *p_i2s_obj[I2S_NUM_MAX] = {0};
static i2s_dev_t *I2S[I2S_NUM_MAX] = {&I2S0, &I2S1};
// static portMUX_TYPE i2s_spinlock[I2S_NUM_MAX] = {portMUX_INITIALIZER_UNLOCKED, portMUX_INITIALIZER_UNLOCKED};

static esp_err_t i2s_isr_register(i2s_port_t i2s_num, int intr_alloc_flags, void (*fn)(void *), void *arg, i2s_isr_handle_t *handle)
{
    return esp_intr_alloc(ETS_I2S0_INTR_SOURCE + i2s_num, intr_alloc_flags, fn, arg, handle);
}

static esp_err_t i2s_set_parallel_mode(i2s_port_t i2s_num)
{
    i2s_dma_t *dma = NULL;
    if ((dma = (i2s_dma_t *)malloc(sizeof(i2s_dma_t))) == NULL) {
        ESP_LOGE(I2S_TAG, "malloc i2s_dma_t fail");
        return ESP_FAIL;
    }
    if ((dma->desc = (lldesc_t **)malloc(sizeof(lldesc_t *))) == NULL) {
        ESP_LOGE(I2S_TAG, "malloc lldesc_t* fail");
        goto _err;
    }
    if ((*(dma->desc) = (lldesc_t *)malloc(sizeof(lldesc_t))) == NULL) {
        ESP_LOGE(I2S_TAG, "malloc lldesc_t fail");
        goto _err;
    }
    memset(*(dma->desc), 0, sizeof(lldesc_t));
    (*(dma->desc))->sosf = 1;
    (*(dma->desc))->eof = 1;
    (*(dma->desc))->owner = 1;
    if ((dma->buf = (char **)malloc(sizeof(char *) * 2)) == NULL) {
        ESP_LOGE(I2S_TAG, "malloc dma buf fail");
        goto _err;
    }

    // In lcd mode, we should allocate a buffer for dma, and configure clk for lcd mode.
    char *buff = (char *)malloc(sizeof(uint32_t) * DMA_SIZE);
    if (buff == NULL) {
        ESP_LOGE(I2S_TAG, "malloc dma buf fail");
        goto _err;
    }
    memset(buff, 0, sizeof(uint32_t) * DMA_SIZE);
    dma->buf[0] = buff;
    dma->buf[1] = buff + DMA_SIZE * 2;
    (*(dma->desc))->buf = (uint8_t *)(dma->buf[0]);
    dma->buf_size = DMA_SIZE / 2;
    p_i2s_obj[i2s_num]->tx = dma;
    p_i2s_obj[i2s_num]->dma_buf_count = 1;
    p_i2s_obj[i2s_num]->dma_buf_len = sizeof(uint32_t) * DMA_SIZE / 2;

    //configure clk of lcd mode, 10M
    I2S[i2s_num]->sample_rate_conf.tx_bck_div_num = 2;
    I2S[i2s_num]->sample_rate_conf.rx_bck_div_num = 2;
    // Change clk by modifying the value of clkm_div_num(4->10M, 8->5M ...)
    // but do not be less than 4.
    I2S[i2s_num]->clkm_conf.clkm_div_b = 1;
    I2S[i2s_num]->clkm_conf.clkm_div_a = 0;
    I2S[i2s_num]->clkm_conf.clkm_div_num = 2;
    I2S[i2s_num]->clkm_conf.clk_en = 1;

    I2S[i2s_num]->int_ena.out_eof = 1;
    I2S[i2s_num]->int_ena.out_dscr_err = 1;

    esp_intr_enable(p_i2s_obj[i2s_num]->i2s_isr_handle);
    dma->queue = xQueueCreate(p_i2s_obj[i2s_num]->dma_buf_count, sizeof(char *));
    dma->mux = xSemaphoreCreateMutex();
    xSemaphoreGive(dma->mux);
    return ESP_OK;

_err:
    if (dma->buf) {
        free(dma->buf);
    }
    if (*(dma->desc)) {
        free(*(dma->desc));
    }
    if (dma->desc) {
        free(dma->desc);
    }
    if (dma) {
        free(dma);
    }
    return ESP_FAIL;
}

static void IRAM_ATTR i2s_intr_handler_default(void *arg)
{
    i2s_obj_t *p_i2s = (i2s_obj_t *) arg;
    uint8_t i2s_num = p_i2s->i2s_num;
    i2s_dev_t *i2s_reg = I2S[i2s_num];
    int dummy;
    portBASE_TYPE high_priority_task_awoken = 0;
    lldesc_t *finish_desc;
    if (i2s_reg->int_st.out_dscr_err || i2s_reg->int_st.in_dscr_err) {
        ESP_EARLY_LOGE(I2S_TAG, "dma error, interrupt status: 0x%08x", i2s_reg->int_st.val);
    }
    if (i2s_reg->int_st.out_eof && p_i2s->tx) {
        finish_desc = (lldesc_t *) i2s_reg->out_eof_des_addr;
        // All buffers are empty. This means we have an underflow on our hands.
        if (xQueueIsQueueFullFromISR(p_i2s->tx->queue)) {
            xQueueReceiveFromISR(p_i2s->tx->queue, &dummy, &high_priority_task_awoken);
        }
        xQueueSendFromISR(p_i2s->tx->queue, (void *)(&finish_desc->buf), &high_priority_task_awoken);
    }
    if (high_priority_task_awoken == pdTRUE) {
        portYIELD_FROM_ISR();
    }
    i2s_reg->int_clr.val = I2S[i2s_num]->int_st.val;
}

static esp_err_t i2s_lcd_config(i2s_port_t i2s_num)
{
    I2S_CHECK((i2s_num < I2S_NUM_MAX), "i2s_num error", ESP_ERR_INVALID_ARG);
    if (i2s_num == I2S_NUM_1) {
        periph_module_enable(PERIPH_I2S1_MODULE);
    } else {
        periph_module_enable(PERIPH_I2S0_MODULE);
    }
    I2S[i2s_num]->conf.rx_fifo_reset = 1;
    I2S[i2s_num]->conf.rx_fifo_reset = 0;
    I2S[i2s_num]->conf.tx_fifo_reset = 1;
    I2S[i2s_num]->conf.tx_fifo_reset = 0;
    //reset i2s
    I2S[i2s_num]->conf.tx_reset = 1;
    I2S[i2s_num]->conf.tx_reset = 0;
    I2S[i2s_num]->conf.rx_reset = 1;
    I2S[i2s_num]->conf.rx_reset = 0;
    //reset dma
    I2S[i2s_num]->lc_conf.in_rst = 1;
    I2S[i2s_num]->lc_conf.in_rst = 0;
    I2S[i2s_num]->lc_conf.out_rst = 1;
    I2S[i2s_num]->lc_conf.out_rst = 0;
    //Enable and configure DMA
    I2S[i2s_num]->lc_conf.check_owner = 0;
    I2S[i2s_num]->lc_conf.out_loop_test = 0;
    I2S[i2s_num]->lc_conf.out_auto_wrback = 0;
    I2S[i2s_num]->lc_conf.out_data_burst_en = 1;
    I2S[i2s_num]->lc_conf.out_no_restart_clr = 0;
    I2S[i2s_num]->lc_conf.indscr_burst_en = 0;
    I2S[i2s_num]->lc_conf.out_eof_mode = 1;

    I2S[i2s_num]->pdm_conf.pcm2pdm_conv_en = 0;
    I2S[i2s_num]->pdm_conf.pdm2pcm_conv_en = 0;

    I2S[i2s_num]->fifo_conf.dscr_en = 0;

    I2S[i2s_num]->conf.tx_start = 0;
    I2S[i2s_num]->conf.rx_start = 0;

    I2S[i2s_num]->conf.tx_slave_mod = 0;
    I2S[i2s_num]->conf.tx_right_first = 1;
    I2S[i2s_num]->conf1.tx_stop_en = 1;
    I2S[i2s_num]->conf1.tx_pcm_bypass = 1;
    I2S[i2s_num]->conf2.lcd_en = 1;
    I2S[i2s_num]->conf_chan.tx_chan_mod = 2;
    I2S[i2s_num]->fifo_conf.tx_fifo_mod = 0;
    I2S[i2s_num]->fifo_conf.tx_fifo_mod_force_en = 1;
    I2S[i2s_num]->sample_rate_conf.tx_bits_mod = 32;
    I2S[i2s_num]->lc_conf.outdscr_burst_en = 1;
    I2S[i2s_num]->conf.tx_msb_right = 1;

    return ESP_OK;
}

static esp_err_t i2s_lcd_driver_install(i2s_port_t i2s_num)
{
    esp_err_t err;
    I2S_CHECK((i2s_num < I2S_NUM_MAX), "i2s_num error", ESP_ERR_INVALID_ARG);
    if (p_i2s_obj[i2s_num] == NULL) {
        p_i2s_obj[i2s_num] = (i2s_obj_t *) malloc(sizeof(i2s_obj_t));
        if (p_i2s_obj[i2s_num] == NULL) {
            ESP_LOGE(I2S_TAG, "Malloc I2S driver error");
            return ESP_ERR_NO_MEM;
        }
        memset(p_i2s_obj[i2s_num], 0, sizeof(i2s_obj_t));
        p_i2s_obj[i2s_num]->i2s_num = i2s_num;
        //To make sure hardware is enabled before any hardware register operations.
        if (i2s_num == I2S_NUM_1) {
            periph_module_enable(PERIPH_I2S1_MODULE);
        } else {
            periph_module_enable(PERIPH_I2S0_MODULE);
        }

        //initial interrupt
        err = i2s_isr_register(i2s_num, 0, i2s_intr_handler_default, p_i2s_obj[i2s_num], &p_i2s_obj[i2s_num]->i2s_isr_handle);
        if (err != ESP_OK) {
            free(p_i2s_obj[i2s_num]);
            p_i2s_obj[i2s_num] = NULL;
            ESP_LOGE(I2S_TAG, "Register I2S Interrupt error");
            return err;
        }
        I2S[i2s_num]->int_ena.val = 0;
        I2S[i2s_num]->out_link.stop = 1;
        I2S[i2s_num]->conf.tx_start = 0;
        err = i2s_lcd_config(i2s_num);
        if (err != ESP_OK) {
            i2s_driver_uninstall(i2s_num);
            ESP_LOGE(I2S_TAG, "I2S param configure error");
            return err;
        }
        //configure camera or lcd mode.
        err = i2s_set_parallel_mode(i2s_num);
        if (err != ESP_OK) {
            i2s_driver_uninstall(i2s_num);
            ESP_LOGE(I2S_TAG, "I2S param configure error");
            return err;
        }
        return ESP_OK;
    }
    ESP_LOGW(I2S_TAG, "I2S driver already installed");
    return ESP_OK;
}

i2s_lcd_handle_t i2s_lcd_create(i2s_port_t i2s_num, i2s_lcd_config_t *pin_conf)
{
    I2S_CHECK((i2s_num < I2S_NUM_MAX), "i2s_num error", NULL);
    i2s_lcd_t* i2s_lcd = (i2s_lcd_t*) calloc(1, sizeof(i2s_lcd_t));
    i2s_lcd->i2s_lcd_conf = *pin_conf;
    i2s_lcd->i2s_port = i2s_num;
    ESP_LOGI(I2S_TAG, "i2s_lcd_create I2S NUM:%d", i2s_num);
    esp_err_t ret = i2s_lcd_driver_install(i2s_num);
    if(ret != ESP_OK) {
        goto error;
    }
    gpio_config_t gpio_conf = {0};
    uint64_t pin_mask = ((uint64_t)1 << pin_conf->ws_io_num);
    uint32_t data_idx = 0;
    for (int i = 0; i < pin_conf->data_width; i++) {
        pin_mask |= ((uint64_t)1 << pin_conf->data_io_num[i]);
    }
    gpio_conf.pin_bit_mask = pin_mask;
    gpio_conf.mode =  GPIO_MODE_OUTPUT;
    gpio_conf.pull_up_en = 0;
    gpio_conf.intr_type = GPIO_INTR_DISABLE;
    if (gpio_config(&gpio_conf) != ESP_OK) {
        return NULL;
    }
    data_idx = (i2s_num == I2S_NUM_0) ? I2S0O_DATA_OUT8_IDX : I2S1O_DATA_OUT8_IDX;
    for (int i = 0; i < pin_conf->data_width; i++) {
        gpio_matrix_out(pin_conf->data_io_num[i], data_idx + i, false, false);
    }
    if (i2s_num == I2S_NUM_0) {
        gpio_matrix_out(pin_conf->ws_io_num, I2S0O_WS_OUT_IDX, true, false);
    } else {
        gpio_matrix_out(pin_conf->ws_io_num, I2S1O_WS_OUT_IDX, true, false);
    }
    return (i2s_lcd_handle_t)i2s_lcd;

    error:
    if(i2s_lcd) {
        free(i2s_lcd);
    }
    return NULL;
}

#ifdef  CONFIG_BIT_MODE_8BIT

int i2s_lcd_write_data(i2s_lcd_handle_t i2s_lcd_handle, const char *src, size_t size, TickType_t ticks_to_wait, bool swap)
{
    i2s_lcd_t* i2s_lcd = (i2s_lcd_t*) i2s_lcd_handle;
    i2s_port_t i2s_num = i2s_lcd->i2s_port;
    I2S_CHECK((i2s_num < I2S_NUM_MAX), "i2s_num error", ESP_ERR_INVALID_ARG);
    uint8_t tagle = 0x01;
    uint8_t *ptr = (uint8_t *)src;
    size_t size_remain = size;
    size_t write_cnt = 0;
    lldesc_t *desc = *(p_i2s_obj[i2s_num]->tx->desc);
    uint32_t *buf = NULL;
    uint32_t loop_cnt = 0;
    xSemaphoreTake(p_i2s_obj[i2s_num]->tx->mux, (portTickType)portMAX_DELAY);
    buf = (uint32_t *)(p_i2s_obj[i2s_num]->tx->buf[tagle ^ 0x1]);
    write_cnt = size_remain > p_i2s_obj[i2s_num]->tx->buf_size ? p_i2s_obj[i2s_num]->tx->buf_size : size_remain;
    for (loop_cnt = 0; loop_cnt < write_cnt; loop_cnt += 2) {
        if (swap) {
            buf[loop_cnt] = ptr[loop_cnt+1];
            buf[loop_cnt+1] = ptr[loop_cnt];
        } else {
            buf[loop_cnt] = ptr[loop_cnt];
            buf[loop_cnt+1] = ptr[loop_cnt+1];
        }
    }
    ptr += write_cnt;
    size_remain -= write_cnt;
    while (1) {
        desc->buf = (uint8_t *)(p_i2s_obj[i2s_num]->tx->buf[tagle ^ 0x1]);
        desc->length = write_cnt * sizeof(uint32_t);
        desc->size = write_cnt * sizeof(uint32_t);
        I2S[i2s_num]->out_link.addr = ((uint32_t)(desc))&I2S_OUTLINK_ADDR;
        I2S[i2s_num]->out_link.start = 1;
        I2S[i2s_num]->fifo_conf.dscr_en = 1;
        I2S[i2s_num]->conf.tx_start = 1;
        tagle ^= 0x1;
        buf = (uint32_t *)(p_i2s_obj[i2s_num]->tx->buf[tagle ^ 0x1]);
        write_cnt = size_remain > p_i2s_obj[i2s_num]->tx->buf_size ? p_i2s_obj[i2s_num]->tx->buf_size : size_remain;
        for (loop_cnt = 0; loop_cnt < write_cnt; loop_cnt += 2) {
            if (swap) {
                buf[loop_cnt] = ptr[loop_cnt+1];
                buf[loop_cnt+1] = ptr[loop_cnt];
            } else {
                buf[loop_cnt] = ptr[loop_cnt];
                buf[loop_cnt+1] = ptr[loop_cnt+1];
            }
        }
        ptr += write_cnt;
        if (xQueueReceive(p_i2s_obj[i2s_num]->tx->queue, &p_i2s_obj[i2s_num]->tx->curr_ptr, ticks_to_wait) == pdFALSE) {
            break;
        }
        I2S[i2s_num]->conf.tx_start = 0;
        I2S[i2s_num]->conf.tx_reset = 1;
        I2S[i2s_num]->conf.tx_reset = 0;
        if (size_remain == 0) {
            break;
        }
        size_remain -= write_cnt;
    }
    xSemaphoreGive(p_i2s_obj[i2s_num]->tx->mux);
    I2S[i2s_num]->fifo_conf.dscr_en = 0;
    return size - size_remain;
}

#else //CONFIG_BIT_MODE_16BIT

int i2s_lcd_write_data(i2s_lcd_handle_t i2s_lcd_handle, const char *src, size_t size, TickType_t ticks_to_wait, bool swap)
{
    i2s_lcd_t* i2s_lcd = (i2s_lcd_t*) i2s_lcd_handle;
    i2s_port_t i2s_num = i2s_lcd->i2s_port;
    I2S_CHECK((i2s_num < I2S_NUM_MAX), "i2s_num error", ESP_ERR_INVALID_ARG);
    uint8_t tagle = 0x01;
    uint16_t *ptr = (uint16_t *)src;
    size_t size_remain = size / 2;
    size_t write_cnt = 0;
    lldesc_t *desc = *(p_i2s_obj[i2s_num]->tx->desc);
    uint32_t *buf = NULL;
    uint32_t loop_cnt = 0;
    xSemaphoreTake(p_i2s_obj[i2s_num]->tx->mux, (portTickType)portMAX_DELAY);
    buf = (uint32_t *)(p_i2s_obj[i2s_num]->tx->buf[tagle ^ 0x1]);
    write_cnt = size_remain > p_i2s_obj[i2s_num]->tx->buf_size ? p_i2s_obj[i2s_num]->tx->buf_size : size_remain;
    for (loop_cnt = 0; loop_cnt < write_cnt; loop_cnt++) {
        buf[loop_cnt] = ptr[loop_cnt];
    }
    ptr += write_cnt;
    size_remain -= write_cnt;
    while (1) {
        desc->buf = (uint8_t *)(p_i2s_obj[i2s_num]->tx->buf[tagle ^ 0x1]);
        desc->length = write_cnt * sizeof(uint32_t);
        desc->size = write_cnt * sizeof(uint32_t);
        I2S[i2s_num]->out_link.addr = ((uint32_t)(desc))&I2S_OUTLINK_ADDR;
        I2S[i2s_num]->out_link.start = 1;
        I2S[i2s_num]->fifo_conf.dscr_en = 1;
        I2S[i2s_num]->conf.tx_start = 1;
        tagle ^= 0x1;
        buf = (uint32_t *)(p_i2s_obj[i2s_num]->tx->buf[tagle ^ 0x1]);
        write_cnt = size_remain > p_i2s_obj[i2s_num]->tx->buf_size ? p_i2s_obj[i2s_num]->tx->buf_size : size_remain;
        for (loop_cnt = 0; loop_cnt < write_cnt; loop_cnt++) {
            buf[loop_cnt] = ptr[loop_cnt];
        }
        ptr += write_cnt;
        if (xQueueReceive(p_i2s_obj[i2s_num]->tx->queue, &p_i2s_obj[i2s_num]->tx->curr_ptr, ticks_to_wait) == pdFALSE) {
            break;
        }
        I2S[i2s_num]->conf.tx_start = 0;
        I2S[i2s_num]->conf.tx_reset = 1;
        I2S[i2s_num]->conf.tx_reset = 0;
        if (size_remain == 0) {
            break;
        }
        size_remain -= write_cnt;
    }
    xSemaphoreGive(p_i2s_obj[i2s_num]->tx->mux);
    I2S[i2s_num]->fifo_conf.dscr_en = 0;
    return size - (size_remain * 2);
}

#endif // CONFIG_BIT_MODE_16BIT
