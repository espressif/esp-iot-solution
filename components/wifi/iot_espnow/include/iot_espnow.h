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
#ifndef _IOT_ESPNOW_H_
#define _IOT_ESPNOW_H_

#include "esp_now.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief  init espnow
 *
 * @return
 *     - ESP_OK
 *     - ESP_FAIL
 */
esp_err_t iot_espnow_init(void);

/**
 * @brief  deinit espnow
 *
 * @return
 *     - ESP_OK
 *     - ESP_FAIL
 */
esp_err_t iot_espnow_deinit(void);

/**
 * @brief  add a peer to espnow peer list based on esp_now_add_peer(...). It is convenient to use simplified MACRO follows.
 *
 * @attention 1. If the peer is exist, it will be deleted firstly.
 * @attention 2. If default_encrypt is true, use default lmk to encrypt espnow data;
 *               if default_encrypt is false and lmk is set, use customized lmk to encrypt espnow data;
 *               otherwise, not encrypt.
 *
 * @param  dest_addr       peer mac address
 * @param  default_encrypt whether to encrypt data with default IOT_ESPNOW_LMK
 * @param  lmk             local master key
 *
 * @return
 *     - ESP_OK
 *     - ESP_FAIL
 */
esp_err_t iot_espnow_add_peer_base(const uint8_t dest_addr[ESP_NOW_ETH_ALEN],
                                   bool default_encrypt, const uint8_t lmk[ESP_NOW_KEY_LEN],
                                   wifi_interface_t interface, int8_t channel);
#define iot_espnow_add_peer(dest_addr, lmk, interface, channel)            iot_espnow_add_peer_base(dest_addr, false, lmk, interface, channel)
#define iot_espnow_add_peer_no_encrypt(dest_addr, interface, channel)      iot_espnow_add_peer_base(dest_addr, false, NULL, interface, channel)
#define iot_espnow_add_peer_default_encrypt(dest_addr, interface, channel) iot_espnow_add_peer_base(dest_addr, true, NULL, interface, channel)

/**
 * @brief  delete a peer from espnow peer list.
 *
 * @param  dest_addr peer mac address
 *
 * @return
 *     - ESP_OK
 *     - ESP_FAIL
 */
esp_err_t iot_espnow_del_peer(const uint8_t dest_addr[ESP_NOW_ETH_ALEN]);

/**
 * @brief  receive data from espnow.
 * 
 * @attention 1. when the data sent from source is encrypt,
 *               the device will not receive befor adding the source into espnow peer.
 *
 * @param  src_addr    source address
 * @param  data        point to received data buffer
 * @param  block_ticks block ticks
 *
 * @return data_len: actually received data length
 *         - ESP_FAIL: read fail
 */
size_t iot_espnow_recv(uint8_t src_addr[ESP_NOW_ETH_ALEN], void *data, 
                            TickType_t block_ticks);

/**
 * @brief  send date package to espnow.
 * 
 * @attention 1. It is necessary to add device to espnow_peer befor send data to dest_addr.
 * @attention 2. When data_len to write is too long, it may fail duration some package and
 *               and the return value is the data len that actually sended.
 *
 * @param  dest_addr   destination address
 * @param  data        point to send data buffer
 * @param  data_len    send data len
 * @param  block_ticks block ticks
 *
 * @return data_len: send data len
 *         - ESP_FAIL: write fail
 */
size_t iot_espnow_send(const uint8_t dest_addr[ESP_NOW_ETH_ALEN], const void *data, 
                            size_t data_len, TickType_t block_ticks);

#ifdef __cplusplus
}
#endif

#endif
