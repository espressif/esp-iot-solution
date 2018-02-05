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

#ifndef _DRIVER_RMT_CTRL_H_
#define _DRIVER_RMT_CTRL_H_
#include "esp_err.h"
#include "soc/rmt_reg.h"
#include "soc/dport_reg.h"
#include "soc/rmt_struct.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/xtensa_api.h"
#include "freertos/ringbuf.h"
#include "driver/gpio.h"
#include "driver/periph_ctrl.h"

#ifdef __cplusplus
extern "C" {
#endif

#define RMT_MEM_BLOCK_BYTE_NUM (256)
#define RMT_MEM_ITEM_NUM  (RMT_MEM_BLOCK_BYTE_NUM/4)

typedef enum {
    RMT_CHANNEL_0 = 0, /*!< RMT Channel 0 */
    RMT_CHANNEL_1,     /*!< RMT Channel 1 */
    RMT_CHANNEL_2,     /*!< RMT Channel 2 */
    RMT_CHANNEL_3,     /*!< RMT Channel 3 */
    RMT_CHANNEL_4,     /*!< RMT Channel 4 */
    RMT_CHANNEL_5,     /*!< RMT Channel 5 */
    RMT_CHANNEL_6,     /*!< RMT Channel 6 */
    RMT_CHANNEL_7,     /*!< RMT Channel 7 */
    RMT_CHANNEL_MAX
} rmt_channel_t;

typedef enum {
    RMT_DATA_MODE_FIFO = 0,  /*<! RMT memory access in FIFO mode */
    RMT_DATA_MODE_MEM = 1,   /*<! RMT memory access in memory mode */
    RMT_DATA_MODE_MAX,
} rmt_data_mode_t;

typedef enum {
    RMT_MODE_TX = 0,   /*!< RMT TX mode */
    RMT_MODE_RX,       /*!< RMT RX mode */
    RMT_MODE_MAX
} rmt_mode_t;

typedef enum {
    RMT_IDLE_LEVEL_LOW = 0,   /*!< RMT TX idle level: low Level */
    RMT_IDLE_LEVEL_HIGH,      /*!< RMT TX idle level: high Level */
    RMT_IDLE_LEVEL_MAX,
} rmt_idle_level_t;

typedef enum {
    RMT_CARRIER_LEVEL_LOW = 0,  /*!< RMT carrier wave is modulated for low Level output */
    RMT_CARRIER_LEVEL_HIGH,     /*!< RMT carrier wave is modulated for high Level output */
    RMT_CARRIER_LEVEL_MAX
} rmt_carrier_level_t;

/**
 * @brief Data struct of RMT TX configure parameters
 */
typedef struct {
    bool loop_en;                         /*!< Enable sending RMT items in a loop */
    uint32_t carrier_freq_hz;             /*!< RMT carrier frequency */
    uint8_t carrier_duty_percent;         /*!< RMT carrier duty (%) */
    rmt_carrier_level_t carrier_level;    /*!< Level of the RMT output, when the carrier is applied */
    bool carrier_en;                      /*!< RMT carrier enable */
    rmt_idle_level_t idle_level;          /*!< RMT idle level */
    bool idle_output_en;                  /*!< RMT idle level output enable */
}rmt_tx_config_t;

/**
 * @brief Data struct of RMT RX configure parameters
 */
typedef struct {
    bool filter_en;                    /*!< RMT receiver filter enable */
    uint8_t filter_ticks_thresh;       /*!< RMT filter tick number */
    uint16_t idle_threshold;           /*!< RMT RX idle threshold */
}rmt_rx_config_t;

/**
 * @brief Data struct of RMT configure parameters
 */
typedef struct {
    rmt_mode_t rmt_mode;               /*!< RMT mode: transmitter or receiver */
    rmt_channel_t channel;             /*!< RMT channel */
    uint8_t clk_div;                   /*!< RMT channel counter divider */
    gpio_num_t gpio_num;               /*!< RMT GPIO number */
    uint8_t mem_block_num;             /*!< RMT memory block number */
    union{
        rmt_tx_config_t tx_config;     /*!< RMT TX parameter */
        rmt_rx_config_t rx_config;     /*!< RMT RX parameter */
    };
} rmt_config_t;

typedef intr_handle_t rmt_isr_handle_t;

typedef void (*rmt_tx_end_fn_t)(rmt_channel_t channel, void *arg);

/**
 * @brief Structure encapsulating a RMT TX end callback
 */
typedef struct {
    rmt_tx_end_fn_t function; /*!< Function which is called on RMT TX end */
    void *arg;                /*!< Optional argument passed to function */
} rmt_tx_end_callback_t;

/**
 * @brief Initialize RMT channel for sending or receive.
 *
 * @param rmt_param RMT parameter struct
 *
 * @return
 *     - ESP_ERR_INVALID_ARG Parameter error
 *     - ESP_OK Success
 */
esp_err_t ir_nec_init(rmt_config_t ir_nec);

/**
 * @brief Delete RMT channel.
 *
 * @param channel RMT channel (0-7)
 *
 * @return
 *     - ESP_ERR_INVALID_ARG Parameter error
 *     - ESP_OK Success
 */
esp_err_t ir_nec_delete(rmt_channel_t channel);

/**
 * @brief Send RMT data.
 *
 * @param channel RMT channel (0-7)
 *
 * @param addr The address for the data waiting to send
 *
 * @param cmd  The data waiting to send
 *
 * @return
 *     - ESP_ERR_INVALID_ARG Parameter error
 *     - ESP_OK Success
 */
esp_err_t ir_nec_send(rmt_channel_t channel, uint16_t addr, uint16_t cmd);

/**
 * @brief Receive RMT data.
 *
 * @param channel RMT channel (0-7)
 *
 * @return
 *     - ESP_ERR_INVALID_ARG Parameter error
 *     - ESP_OK Success
 */
uint16_t ir_nec_recv(rmt_channel_t channel);



/*
 * ----------------EXAMPLE OF RMT INTERRUPT ------------------
 * @code{c}
 *
 * rmt_isr_register(rmt_isr, NULL, 0);           //hook the ISR handler for RMT interrupt
 * @endcode
 * @note
 *     0. If you have called rmt_driver_install, you don't need to set ISR handler any more.
 *
 * ----------------EXAMPLE OF INTERRUPT HANDLER ---------------
 * @code{c}
 * #include "esp_attr.h"
 * void IRAM_ATTR rmt_isr_handler(void* arg)
 * {
 *    //read RMT interrupt status.
 *    uint32_t intr_st = RMT.int_st.val;
 *
 *    //you will find which channels have triggered an interrupt here,
 *    //then, you can post some event to RTOS queue to process the event.
 *    //later we will add a queue in the driver code.
 *
 *   //clear RMT interrupt status.
 *    RMT.int_clr.val = intr_st;
 * }
 * @endcode
 *
 *--------------------------END OF EXAMPLE --------------------------
 */



#ifdef __cplusplus
}
#endif

#endif /* _DRIVER_RMT_CTRL_H_ */
