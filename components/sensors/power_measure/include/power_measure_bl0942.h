/*
 * SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO LTD
*
* SPDX-License-Identifier: Apache-2.0
*/

#ifndef __POWER_MEASURE_BL0942__
#define __POWER_MEASURE_BL0942__

#include "driver/uart.h"
#include "driver/spi_common.h"
#include "power_measure.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief BL0942 specific configuration
 */
typedef struct {
    uint8_t addr;                   /*!< Device address [A2,A1] 0..3 (SSOP10L=0) */
    float shunt_resistor;           /*!< Shunt resistor (Ohm) */
    float divider_ratio;            /*!< Voltage divider ratio (line = pin*ratio) */
    bool use_spi;                   /*!< true to use SPI, false to use UART */

    union {
        struct {
            uart_port_t uart_num;           /*!< UART port */
            int tx_io;                      /*!< UART TX GPIO */
            int rx_io;                      /*!< UART RX GPIO */
            int sel_io;                     /*!< SEL GPIO (UART mode: pull low), -1 if HW tied */
            uint32_t baud;                  /*!< Baudrate: 4800/9600/19200/38400 */
        } uart;

        struct {
            spi_host_device_t spi_host;     /*!< SPI host when use_spi=true */
            int mosi_io;                    /*!< SPI MOSI GPIO */
            int miso_io;                    /*!< SPI MISO GPIO */
            int sclk_io;                    /*!< SPI SCLK GPIO */
            int cs_io;                      /*!< SPI CS GPIO (-1 if external) */
        } spi;
    };
} power_measure_bl0942_config_t;

/**
 * @brief Create a new BL0942 power measurement device
 *
 * @param config Common power measure configuration
 * @param bl0942_config BL0942 specific configuration
 * @param ret_handle Pointer to store the created device handle
 * @return
 *     - ESP_OK: Successfully created the device
 *     - ESP_ERR_INVALID_ARG: Invalid arguments
 *     - ESP_ERR_NO_MEM: Memory allocation failed
 *     - ESP_ERR_INVALID_STATE: Device already initialized
 */
esp_err_t power_measure_new_bl0942_device(const power_measure_config_t *config,
                                          const power_measure_bl0942_config_t *bl0942_config,
                                          power_measure_handle_t *ret_handle);

#ifdef __cplusplus
}
#endif

#endif /* __POWER_MEASURE_BL0942__ */
