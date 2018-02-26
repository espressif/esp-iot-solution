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
#ifdef __cplusplus
extern "C" {
#endif

#include <esp_types.h>
#include <string.h>
#include <stdlib.h>
#include "esp_log.h"
#include "esp_err.h"
#include "driver/rmt.h"
#include "iot_ir.h"
#include "freertos/ringbuf.h"


#define NEC_HEADER_HIGH_US    9000                         /*!< NEC protocol header: positive 9ms */
#define NEC_HEADER_LOW_US     4500                         /*!< NEC protocol header: negative 4.5ms*/
#define NEC_BIT_ONE_HIGH_US    560                         /*!< NEC protocol data bit 1: positive 0.56ms */
#define NEC_BIT_ONE_LOW_US    (2250-NEC_BIT_ONE_HIGH_US)   /*!< NEC protocol data bit 1: negative 1.69ms */
#define NEC_BIT_ZERO_HIGH_US   560                         /*!< NEC protocol data bit 0: positive 0.56ms */
#define NEC_BIT_ZERO_LOW_US   (1120-NEC_BIT_ZERO_HIGH_US)  /*!< NEC protocol data bit 0: negative 0.56ms */
#define NEC_BIT_END            560                         /*!< NEC protocol end: positive 0.56ms */
#define NEC_BIT_MARGIN         20                          /*!< NEC parse margin time */

#define NEC_DATA_ITEM_NUM   34  /*!< NEC code item number: header + 32bit data + end */
#define RMT_TX_DATA_NUM  1    /*!< NEC tx test data number */
#define RMT_CLK_DIV      100    /*!< RMT counter clock divider */
#define RMT_TICK_10_US    (80000000/RMT_CLK_DIV/100000)   /*!< RMT counter value for 10 us.(Source clock is APB clock) */
#define NEC_ITEM_DURATION(d)  ((d & 0x7fff)*10/RMT_TICK_10_US)  /*!< Parse duration time from memory register value */
#define RMT_NEC_TIMEOUT_US  9500   /*!< RMT receiver timeout value(us) */

static const char* IR_NEC_TAG = "ir_nec";

#define IR_NEC_CHECK(a, str, ret_val) \
    if (!(a)) { \
        ESP_LOGE(IR_NEC_TAG,"%s(%d): %s", __FUNCTION__, __LINE__, str); \
        return (ret_val); \
    }

/*
 * @brief Build register value of waveform for NEC one data bit
 */
static inline void nec_fill_item_level(rmt_item32_t* item, int high_us, int low_us)
{
    item->level0 = 1;
    item->duration0 = (high_us) / 10 * RMT_TICK_10_US;
    item->level1 = 0;
    item->duration1 = (low_us) / 10 * RMT_TICK_10_US;
}

/*
 * @brief Generate NEC header value: active 9ms + negative 4.5ms
 */
static void nec_fill_item_header(rmt_item32_t* item)
{
    nec_fill_item_level(item, NEC_HEADER_HIGH_US, NEC_HEADER_LOW_US);
}

/*
 * @brief Generate NEC data bit 1: positive 0.56ms + negative 1.69ms
 */
static void nec_fill_item_bit_one(rmt_item32_t* item)
{
    nec_fill_item_level(item, NEC_BIT_ONE_HIGH_US, NEC_BIT_ONE_LOW_US);
}

/*
 * @brief Generate NEC data bit 0: positive 0.56ms + negative 0.56ms
 */
static void nec_fill_item_bit_zero(rmt_item32_t* item)
{
    nec_fill_item_level(item, NEC_BIT_ZERO_HIGH_US, NEC_BIT_ZERO_LOW_US);
}

/*
 * @brief Generate NEC end signal: positive 0.56ms
 */
static void nec_fill_item_end(rmt_item32_t* item)
{
    nec_fill_item_level(item, NEC_BIT_END, 0x7fff);
}

/*
 * @brief Check whether duration is around target_us
 */
inline bool nec_check_in_range(int duration_ticks, int target_us, int margin_us)
{
    if(( NEC_ITEM_DURATION(duration_ticks) < (target_us + margin_us))
        && ( NEC_ITEM_DURATION(duration_ticks) > (target_us - margin_us))) {
        return true;
    } else {
        return false;
    }
}

/*
 * @brief Check whether this value represents an NEC header
 */
static bool nec_header_if(rmt_item32_t* item, int active_level)
{
    if((item->level0 == active_level && item->level1 != active_level)
        && nec_check_in_range(item->duration0, NEC_HEADER_HIGH_US, NEC_BIT_MARGIN)
        && nec_check_in_range(item->duration1, NEC_HEADER_LOW_US, NEC_BIT_MARGIN)) {
        return true;
    }
    return false;
}

/*
 * @brief Check whether this value represents an NEC data bit 1
 */
static bool nec_bit_one_if(rmt_item32_t* item, int active_level)
{
    if((item->level0 == active_level && item->level1 != active_level)
        && nec_check_in_range(item->duration0, NEC_BIT_ONE_HIGH_US, NEC_BIT_MARGIN)
        && nec_check_in_range(item->duration1, NEC_BIT_ONE_LOW_US, NEC_BIT_MARGIN)) {
        return true;
    }
    return false;
}

/*
 * @brief Check whether this value represents an NEC data bit 0
 */
static bool nec_bit_zero_if(rmt_item32_t* item, int active_level)
{
    if((item->level0 == active_level && item->level1 != active_level)
        && nec_check_in_range(item->duration0, NEC_BIT_ZERO_HIGH_US, NEC_BIT_MARGIN)
        && nec_check_in_range(item->duration1, NEC_BIT_ZERO_LOW_US, NEC_BIT_MARGIN)) {
        return true;
    }
    return false;
}


/*
 * @brief Parse NEC 32 bit waveform to address and command.
 */
static int nec_parse_items(rmt_item32_t* item, int item_num, uint16_t* addr, uint16_t* data, int active_level)
{
    int w_len = item_num;
    if(w_len < NEC_DATA_ITEM_NUM) {
        return -1;
    }
    int i = 0, j = 0;
    if(!nec_header_if(item++, active_level)) {
        return -1;
    }
    uint16_t addr_t = 0;
    for(j = 0; j < 16; j++) {
        if(nec_bit_one_if(item, active_level)) {
            addr_t |= (1 << j);
        } else if(nec_bit_zero_if(item, active_level)) {
            addr_t |= (0 << j);
        } else {
            return -1;
        }
        item++;
        i++;
    }
    uint16_t data_t = 0;
    for(j = 0; j < 16; j++) {
        if(nec_bit_one_if(item, active_level)) {
            data_t |= (1 << j);
        } else if(nec_bit_zero_if(item, active_level)) {
            data_t |= (0 << j);
        } else {
            return -1;
        }
        item++;
        i++;
    }
    *addr = addr_t;
    *data = data_t;
    return i;
}

/*
 * @brief Build NEC 32bit waveform.
 */
static int nec_build_items(int channel, rmt_item32_t* item, int item_num, uint16_t addr, uint16_t cmd_data)
{
    int i = 0, j = 0;
    if(item_num < NEC_DATA_ITEM_NUM) {
        return -1;
    }
    nec_fill_item_header(item++);
    i++;
    for(j = 0; j < 16; j++) {
        if(addr & 0x1) {
            nec_fill_item_bit_one(item);
        } else {
            nec_fill_item_bit_zero(item);
        }
        item++;
        i++;
        addr >>= 1;
    }
    for(j = 0; j < 16; j++) {
        if(cmd_data & 0x1) {
            nec_fill_item_bit_one(item);
        } else {
            nec_fill_item_bit_zero(item);
        }
        item++;
        i++;
        cmd_data >>= 1;
    }
    nec_fill_item_end(item);
    i++;
    return i;
}

static esp_err_t ir_nec_send(rmt_channel_t channel, uint16_t addr, uint16_t cmd)
{
	size_t size = (sizeof(rmt_item32_t) * NEC_DATA_ITEM_NUM);
	rmt_item32_t* item = (rmt_item32_t*) malloc(size);
    int item_num = NEC_DATA_ITEM_NUM;
    memset((void*) item, 0, size);

	//To build a series of waveforms.
	nec_build_items(channel, item, item_num, ((~addr) << 8) | addr,((~cmd) << 8) | cmd);

    rmt_write_items(channel, item, item_num, true);
    //Wait until sending is done.
    rmt_wait_tx_done(channel, portMAX_DELAY);
    //before we free the data, make sure sending is already done.
    free(item);
    return ESP_OK;
}

static esp_err_t ir_nec_recv(rmt_channel_t channel, uint16_t* addr, uint16_t* cmd, int active_level, TickType_t wait_time)
{
    esp_err_t ret = ESP_FAIL;
    RingbufHandle_t rb = NULL;
    //get RMT RX ringbuffer
    rmt_get_ringbuf_handle(channel, &rb);
    IR_NEC_CHECK(rb != NULL, "IR NEC dev ringbuffer error", ESP_FAIL);
    size_t rx_size = 0;
    //try to receive data from ringbuffer.
    //RMT driver will push all the data it receives to its ringbuffer.
    //We just need to parse the value and return the spaces of ringbuffer.
    rmt_item32_t* item = (rmt_item32_t*) xRingbufferReceive(rb, &rx_size, wait_time);
    IR_NEC_CHECK(item != NULL, "IR NEC dev ringbuffer data error", ESP_FAIL);
    //parse data value from ringbuffer.
    int res = nec_parse_items(item, rx_size / 4, addr, cmd, active_level);
    if(res > 0) {
        ret = ESP_OK;
    } else {
        ret = ESP_FAIL;
    }
    //after parsing the data, return spaces to ringbuffer.
    vRingbufferReturnItem(rb, (void*) item);
    return ret;
}

#ifdef __cplusplus
}
#endif

CIrNecSender::CIrNecSender(rmt_channel_t channel, gpio_num_t io_num, bool carrier_en, ir_proto_t proto)
{
    m_channel = channel;
    m_io_num = io_num;
    m_proto = proto;
    m_carrier_en = carrier_en;
    m_rmt_mode = RMT_MODE_TX;

    rmt_config_t rmt;
    rmt.channel = m_channel;
    rmt.gpio_num = m_io_num;
    rmt.mem_block_num = 1;
    rmt.clk_div = RMT_CLK_DIV;
    int rx_buf_size = 0;
    rmt.rmt_mode = m_rmt_mode;
    rmt.tx_config.loop_en = false;
    rmt.tx_config.carrier_duty_percent = 50;
    rmt.tx_config.carrier_freq_hz = 38000;
    rmt.tx_config.carrier_level = RMT_CARRIER_LEVEL_HIGH;
    rmt.tx_config.carrier_en = m_carrier_en;
    rmt.tx_config.idle_level = RMT_IDLE_LEVEL_LOW;
    rmt.tx_config.idle_output_en = true;
    rmt_config(&rmt);
    rmt_driver_install(rmt.channel, rx_buf_size, 0);
}

esp_err_t CIrNecSender::send(uint16_t addr, uint16_t cmd)
{
    IR_NEC_CHECK(m_rmt_mode == RMT_MODE_TX, "IR NEC dev not in TX mode", ESP_FAIL);
    return ir_nec_send(m_channel, addr, cmd);
}

CIrNecSender::~CIrNecSender()
{
    rmt_driver_uninstall(m_channel);
}

CIrNecRecv::CIrNecRecv(rmt_channel_t channel, gpio_num_t io_num, int active_level, ir_proto_t proto, int rx_buf_size)
{
    m_channel = channel;
    m_io_num = io_num;
    m_proto = proto;
    m_rmt_mode = RMT_MODE_RX;
    m_active_level = active_level;

    rmt_config_t rmt;
    rmt.channel = m_channel;
    rmt.gpio_num = m_io_num;
    rmt.mem_block_num = 1;
    rmt.clk_div = RMT_CLK_DIV;
    rmt.rmt_mode = m_rmt_mode;
    rmt.rx_config.filter_en = true;
    rmt.rx_config.filter_ticks_thresh = 100;
    rmt.rx_config.idle_threshold = RMT_NEC_TIMEOUT_US / 10 * (RMT_TICK_10_US);
    rmt_rx_start(channel, 1);
    rmt_config(&rmt);
    rmt_driver_install(rmt.channel, rx_buf_size, 0);
}

esp_err_t CIrNecRecv::recv(uint16_t *addr, uint16_t *cmd, TickType_t wait_time)
{
    IR_NEC_CHECK(m_rmt_mode == RMT_MODE_RX, "IR NEC dev not in TX mode", ESP_FAIL);
    return ir_nec_recv(m_channel, addr, cmd, m_active_level, wait_time);
}

CIrNecRecv::~CIrNecRecv()
{
    rmt_driver_uninstall(m_channel);
}



