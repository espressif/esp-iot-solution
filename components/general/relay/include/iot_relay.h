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
#ifndef _IOT_RELAY_H_
#define _IOT_RELAY_H_
#include "sdkconfig.h"
#ifdef __cplusplus
extern "C" {
#endif
typedef void* relay_handle_t;

typedef enum {
    RELAY_CLOSE_LOW = 0,            /**< pass this param to iot_relay_create if the relay is closed when control-gpio level is low */
    RELAY_CLOSE_HIGH = 1,           /**< pass this param to iot_relay_create if the relay is closed when control-gpio level is high */
} relay_close_level_t;

typedef enum {
    RELAY_STATUS_CLOSE = 0,
    RELAY_STATUS_OPEN,
} relay_status_t;

typedef enum {
    RELAY_DFLIP_CONTROL = 0,        /**< if the relay is controlled by d flip-flop */
    RELAY_GPIO_CONTROL,             /**< if the relay is controlled by gpio directly */
} relay_ctl_mode_t;

typedef enum {
    RELAY_IO_NORMAL = 0,            /**< use normal gpio */
    RELAY_IO_RTC,                   /**< use rtc io */
} relay_io_mode_t;

typedef struct {
    gpio_num_t d_io_num;            /**< voltage level control pin of d flip-flop */ 
    gpio_num_t cp_io_num;           /**< clock pin of d flip-flop */
} d_flip_io_t;


typedef struct {
    gpio_num_t ctl_io_num;
} relay_single_io_t;

typedef union {
    d_flip_io_t flip_io;            /**< gpio numbers of d flip-flop */
    relay_single_io_t single_io;
} relay_io_t;
    

/**
  * @brief create relay object.
  *
  * @param relay_io gpio number(s) of relay
  * @param close_level close voltage level of relay
  * @param ctl_mode control relay by d flip-flop or gpio
  * @param io_mode gpio work in normal mode or rtc mode
  *
  * @return relay_handle_t the handle of the relay created 
  */
relay_handle_t iot_relay_create(relay_io_t relay_io, relay_close_level_t close_level, relay_ctl_mode_t ctl_mode, relay_io_mode_t io_mode);

/**
  * @brief set state of relay
  *
  * @param  relay_handle
  * @param  state RELAY_STATUS_CLOSE or RELAY_STATUS_OPEN
  *
  * @return
  *     - ESP_OK: succeed
  *     - others: fail
  */
esp_err_t iot_relay_state_write(relay_handle_t relay_handle, relay_status_t state);

/**
  * @brief get state of relay
  *
  * @param relay_handle
  *
  * @return state of the relay
  */
relay_status_t iot_relay_state_read(relay_handle_t relay_handle);

/**
  * @brief free the memory of relay
  *
  * @param  relay_handle
  *
  * @return
  *     - ESP_OK: succeed
  *     - others: fail
  */
esp_err_t iot_relay_delete(relay_handle_t relay_handle);

#ifdef __cplusplus
}
#endif

#ifdef __cplusplus
class CControllable
{
public:
    virtual esp_err_t on() = 0;
    virtual esp_err_t off() = 0;
    virtual ~CControllable() = 0;
};

/**
 * class of relay
 * simple usage:
 * relay_io_t relay_io_0 = {
 *      .flip_io = {
 *          .d_io_num = GPIO_NUM_2,
 *          .cp_io_num = GPIO_NUM_4,
 *      },
 * };
 * CRelay relay_0(relay_io_0, RELAY_CLOSE_HIGH, RELAY_DFLIP_CONTROL, RELAY_IO_NORMAL);
 * relay_0.on();
 */
class CRelay : public CControllable
{
private:
    relay_handle_t m_relay_handle;

    /**
     * prevent copy construct
     */
    CRelay(const CRelay&);
    CRelay& operator = (const CRelay&);

public:
    /**
     * @brief  constructor of CRelay
     *
     * @param  relay_io gpios of relay
     * @param  close_level close voltage level of relay
     * @param  ctl_mode control relay by d flip-flop or gpio
     * @param  io_mode gpio work in normal mode or rtc mode
     */
    CRelay(relay_io_t relay_io, relay_close_level_t close_level, relay_ctl_mode_t ctl_mode, relay_io_mode_t io_mode = RELAY_IO_NORMAL);

    /**
     * @brief  turn on the relay
     *
     * @return
     *     - ESP_OK: succeed
     *     - others: fail
     */
    virtual esp_err_t on();

    /**
     * @brief  turn off the relay
     *
     * @return
     *     - ESP_OK: succeed
     *     - others: fail
     */
    virtual esp_err_t off();

    /**
     * @brief get current status of relay
     *
     * @return status
     */
    relay_status_t status();

    virtual ~CRelay();
};

#endif
#endif
