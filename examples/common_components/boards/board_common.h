/*
 * SPDX-FileCopyrightText: 2022-2023 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef _IOT_BOARDS_COMMON_H_
#define _IOT_BOARDS_COMMON_H_

#include "i2c_bus.h"
#include "spi_bus.h"
#include "iot_button.h"

#define ATTR_WEAK __attribute__((weak))
#define BOARD_CHECK(a, str, ret) if(!(a)) { \
        ESP_LOGE(TAG,"%s:%d (%s):%s", __FILE__, __LINE__, __FUNCTION__, str); \
        return (ret); \
    }

#ifndef _ENABLE
#define _ENABLE 1
#endif

#ifndef _DISABLE
#define _DISABLE 0
#endif

#ifndef _UNDEFINE
#define _UNDEFINE
#endif

#ifndef _POSITIVE
#define _POSITIVE 1
#endif

#ifndef _NEGATIVE
#define _NEGATIVE 0
#endif

#define LED_TYPE_GPIO 0
#define LED_TYPE_RGB 1

typedef void* board_res_handle_t;
typedef esp_err_t (*init_cb_t)(void);
typedef board_res_handle_t (*get_handle_cb_t)(int);

typedef struct {
    init_cb_t pre_init_cb; /*!< Function used for board specific level init, previous before others */
    init_cb_t post_init_cb; /*!< Function used for board specific level init, post after others */
    init_cb_t pre_deinit_cb; /*!< Function used for board specific level deinit, previous before others */
    init_cb_t post_deinit_cb; /*!< Function used for board specific level deinit, post after others */
    get_handle_cb_t get_handle_cb; /*!< Function used for get board specific level resource's handle */
} board_specific_callbacks_t;

#ifdef __cplusplus
extern "C"
{
#endif

/**
 * @brief Board common level init.
 *        Peripherals can be initialized with defaults during board init, through menuconfig.
 *        After board init, initialized peripherals can be referenced by handles directly.
 *
 * @return esp_err_t
 *         ESP_ERR_INVALID_STATE board has been ininted
 *         ESP_OK init succeed
 *         ESP_FAIL init failed
 */
esp_err_t iot_board_init(void);

/**
 * @brief Board common level deinit.
 *        After board deinit, peripherals will be deinited and related handles will be set to NULL.
 *
 * @return esp_err_t
 *         ESP_ERR_INVALID_STATE board has been deininted
 *         ESP_OK deinit succeed
 *         ESP_FAIL deinit failed
 */
esp_err_t iot_board_deinit(void);

/**
 * @brief Using resource's ID declared in board_res_id_t to get common resource's handle
 *
 * @param id resource's ID declared in board_res_id_t
 * @return board_res_handle_t resource's handle
 *         if no related handle, NULL will be returned
 */
board_res_handle_t iot_board_get_handle(int id);

/**
 * @brief Check if board is initialized
 *
 * @return true if board is initialized
 * @return false if board is not initialized
 */
bool iot_board_is_init(void);

/**
 * @brief Register button event callback.
 *
 * @param gpio_num button gpio num defined in iot_board.h
 * @param event button event detailed in button_event_t
 * @param cb event's callback.
 *
 * @return
 *      - ESP_OK register success
 *      - ESP_FAIL register failed, button not inited
 *      - ESP_ERR_INVALID_ARG arguments invalid.
 */
esp_err_t iot_board_button_register_cb(int gpio_num, button_event_t event, button_cb_t cb);

/**
 * @brief Unregister the button event's callback.
 *
 * @param gpio_num button gpio num defined in iot_board.h
 * @param event button event detailed in button_event_t
 *
 * @return
 *      - ESP_OK unregister success
 *      - ESP_ERR_INVALID_ARG arguments invalid
 */
esp_err_t iot_board_button_unregister_cb(int gpio_num, button_event_t event);

/**
 * @brief set led state
 *
 * @param gpio_num led gpio num defined in iot_board.h
 * @param turn_on true if turn on the LED, false if turn off the LED
 * @return esp_err_t
 */
esp_err_t iot_board_led_set_state(int gpio_num, bool turn_on);

/**
 * @brief set all led state
 *
 * @param turn_on true if turn on the LED, false is turn off the LED
 * @return esp_err_t
 */
esp_err_t iot_board_led_all_set_state(bool turn_on);

/**
 * @brief toggle led state
 *
 * @param gpio_num led gpio num defined in iot_board.h
 * @return esp_err_t
 */
esp_err_t iot_board_led_toggle_state(int gpio_num);

/**
 * @brief init Wi-Fi with configs from menuconfig
 *
 * @return esp_err_t
 *         ESP_OK init succeed
 *         ESP_FAIL init failed
 */
esp_err_t iot_board_wifi_init(void);

/**
 * @brief Get board information
 *
 * @return char* board related message.
 */
char* iot_board_get_info();

#ifdef __cplusplus
}
#endif

#endif
