/*
 * SPDX-FileCopyrightText: 2022-2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <stdint.h>
#include <sys/queue.h>
#include "driver/rmt_types.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Type of IR learn handle
 */
typedef void *ir_learn_handle_t;

/**
 * @brief Type of IR learn step
 */
typedef enum {
    IR_LEARN_STATE_STEP,        /**< IR learn step, start from 1 */
    IR_LEARN_STATE_READY = 20,  /**< IR learn ready, after successful initialization */
    IR_LEARN_STATE_END,         /**< IR learn successfully */
    IR_LEARN_STATE_FAIL,        /**< IR learn failure */
    IR_LEARN_STATE_EXIT,        /**< IR learn exit */
} ir_learn_state_t;

/**
 * @brief An element in the list of infrared (IR) learn data packets.
 *
 */
struct ir_learn_sub_list_t {
    uint32_t timediff;                      /*!< The interval time from the previous packet (ms) */
    rmt_rx_done_event_data_t symbols;       /*!< Received RMT symbols */
    SLIST_ENTRY(ir_learn_sub_list_t) next;  /*!< Pointer to the next packet */
};

/**
 * @cond Doxy command to hide preprocessor definitions from docs
 *
 * @brief The head of a singly-linked list for IR learn cmd packets.
 *
 */
SLIST_HEAD(ir_learn_sub_list_head, ir_learn_sub_list_t);
/**
 * @endcond
 */

/**
 * @brief The head of a list of infrared (IR) learn data packets.
 *
 */
struct ir_learn_list_t {
    struct ir_learn_sub_list_head cmd_sub_node; /*!< Package head of every cmd */
    SLIST_ENTRY(ir_learn_list_t) next;  /*!< Pointer to the next packet */
};

/**
 * @cond Doxy command to hide preprocessor definitions from docs
 *
 * @brief The head of a singly-linked list for IR learn data packets.
 *
 */
SLIST_HEAD(ir_learn_list_head, ir_learn_list_t);
/**
 * @endcond
 */

/**
* @brief IR learn result user callback.
*
* @param[out] state IR learn step
* @param[out] sub_step Interval less than 500 ms, we think it's the same command
* @param[out] data Command list of this step
*
*/
typedef void (*ir_learn_result_cb)(ir_learn_state_t state, uint8_t sub_step, struct ir_learn_sub_list_head *data);

/**
 * @brief IR learn configuration
 */
typedef struct {
    rmt_clock_source_t clk_src; /*!< RMT clock source */
    uint32_t resolution;        /*!< RMT resolution, in Hz */

    int learn_count;            /*!< IR learn count needed */
    int learn_gpio;             /*!< IR learn io that consumed by the sensor */
    ir_learn_result_cb callback;/*!< IR learn result callback for user */

    int task_priority;          /*!< IR learn task priority */
    int task_stack;             /*!< IR learn task stack size */
    int task_affinity;          /*!< IR learn task pinned to core (-1 is no affinity) */
} ir_learn_cfg_t;

/**
* @brief Create new IR learn handle.
*
* @param[in]  cfg Config for IR learn
* @param[out] handle_out New IR learn handle
* @return
*          - ESP_OK                  Device handle creation success.
*          - ESP_ERR_INVALID_ARG     Invalid device handle or argument.
*          - ESP_ERR_NO_MEM          Memory allocation failed.
*
*/
esp_err_t ir_learn_new(const ir_learn_cfg_t *cfg, ir_learn_handle_t *handle_out);

/**
* @brief Restart IR learn process.
*
* @param[in] ir_learn_hdl IR learn handle
* @return
*          - ESP_OK                  Restart process success.
*          - ESP_ERR_INVALID_ARG     Invalid device handle or argument.
*
*/
esp_err_t ir_learn_restart(ir_learn_handle_t ir_learn_hdl);

/**
* @brief Stop IR learn process.
* @note Delete all
*
* @param[in] ir_learn_hdl IR learn handle
* @return
*          - ESP_OK                  Stop process success.
*          - ESP_ERR_INVALID_ARG     Invalid device handle or argument.
*
*/
esp_err_t ir_learn_stop(ir_learn_handle_t *ir_learn_hdl);

/**
* @brief Add IR learn list node, every new learn list will create it.
*
* @param[in] learn_head IR learn list head
* @return
*          - ESP_OK                  Create learn list success.
*          - ESP_ERR_NO_MEM          Memory allocation failed.
*
*/
esp_err_t ir_learn_add_list_node(struct ir_learn_list_head *learn_head);

/**
* @brief Add IR learn sub step list node, every sub step should be added.
*
* @param[in] sub_head IR learn sub step list head
* @param[in] timediff Time diff between each sub step
* @param[in] symbol symbols of each sub step
* @return
*          - ESP_OK                  Create learn list success.
*          - ESP_ERR_NO_MEM          Memory allocation failed.
*
*/
esp_err_t ir_learn_add_sub_list_node(struct ir_learn_sub_list_head *sub_head,
                                     uint32_t timediff, const rmt_rx_done_event_data_t *symbol);

/**
* @brief Delete IR learn list node, will recursively delete sub steps.
*
* @param[in] learn_head IR learn list head
*          - ESP_OK                  Stop process success.
*          - ESP_ERR_INVALID_ARG     Invalid device handle or argument.
*
*/
esp_err_t ir_learn_clean_data(struct ir_learn_list_head *learn_head);

/**
* @brief Delete sub steps.
*
* @param[in] sub_head IR learn sub list head
*          - ESP_OK                  Stop process success.
*          - ESP_ERR_INVALID_ARG     Invalid device handle or argument.
*
*/
esp_err_t ir_learn_clean_sub_data(struct ir_learn_sub_list_head *sub_head);

/**
* @brief Add IR learn list node, every new learn list will create it.
*
* @param[in] learn_head IR learn list head
* @param[out] result_out IR learn result
* @return
*          - ESP_OK                  Get learn result process.
*          - ESP_ERR_INVALID_SIZE    Size error.
*
*/
esp_err_t ir_learn_check_valid(struct ir_learn_list_head *learn_head,
                               struct ir_learn_sub_list_head *result_out);

/**
* @brief Print the RMT symbols.
*
* @param[in] cmd_list IR learn list head
*          - ESP_OK                  Stop process success.
*          - ESP_ERR_INVALID_ARG     Invalid device handle or argument.
*
*/
esp_err_t ir_learn_print_raw(struct ir_learn_sub_list_head *cmd_list);

#ifdef __cplusplus
}
#endif
