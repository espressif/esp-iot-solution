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

#ifndef _IOT_IR_NEC_H_
#define _IOT_IR_NEC_H_
#include "esp_err.h"
#include "driver/rmt.h"
#include <stdio.h>
#include "esp_log.h"

#ifdef __cplusplus
typedef enum {
    IR_PROTO_NEC,     /* IR NEC protocol */
    IR_PROTO_MAX,
} ir_proto_t;

class CIrNecSender
{
private:
    rmt_channel_t m_channel;
    gpio_num_t m_io_num;
    ir_proto_t m_proto;
    bool m_carrier_en;
    rmt_mode_t m_rmt_mode;

    /**
     * prevent copy constructing
     */
    CIrNecSender(const CIrNecSender&);
    CIrNecSender& operator =(const CIrNecSender&);
public:
    /**
     * @brief Constructor for CIrNecSender class
     * @param channel RMT hardware channel number
     * @param io_num gpio index for RMT
     * @param ir_proto_t IR protocol
     * @param carrier_en whether to enable carrier for TX mode
     */
    CIrNecSender(rmt_channel_t channel, gpio_num_t io_num, bool carrier_en = true, ir_proto_t proto = IR_PROTO_NEC);

    /*
     * @brief send NEC data waveform
     * @param addr address field to send
     * @param cmd command field to send
     * @return
     *     - ESP_OK if success
     */
    esp_err_t send(uint16_t addr, uint16_t cmd);

    /**
     * @brief Destructor function of CIrNecSender class
     */
    ~CIrNecSender(void);
};

class CIrNecRecv
{
private:
    rmt_channel_t m_channel;
    gpio_num_t m_io_num;
    ir_proto_t m_proto;
    rmt_mode_t m_rmt_mode;
    int m_active_level;

    /**
     * prevent copy constructing
     */
    CIrNecRecv(const CIrNecRecv&);
    CIrNecRecv& operator =(const CIrNecRecv&);
public:
    /**
     * @brief Constructor for CIrNecRecv class
     * @param channel RMT hardware channel number
     * @param io_num gpio index for RMT
     * @param ir_proto_t IR protocol
     * @param carrier_en whether to enable carrier for TX mode
     */
    CIrNecRecv(rmt_channel_t channel, gpio_num_t io_num, int active_level = 0, ir_proto_t proto = IR_PROTO_NEC, int rx_buf_size = 1000);

    /*
     * @brief Receive data
     * @param addr pointer to accept address field
     * @param cmd pointer to accept command field
     * @param wait_time max wait time in tick
     * @return
     *     - ESP_OK if success
     *     - ESP_ERR_TIMEOUT if timeout
     *     - ESP_FAIL if fail
     */
    esp_err_t recv(uint16_t *addr, uint16_t *cmd, TickType_t wait_time);

    /**
     * @brief Destructor function of CIrNecRecv class
     */
    ~CIrNecRecv(void);
};

#endif

#endif /* _IOT_IR_NEC_H_ */
