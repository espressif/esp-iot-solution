// Copyright 2020 Espressif Systems (Shanghai) PTE LTD
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
#pragma once

#ifdef __cplusplus
extern "C" {
#endif


#include "esp_modem.h"
#include "esp_modem_dte.h"
#include "esp_modem_dce.h"

/**
 * @brief Specific Length Constraint
 *
 */
#define MODEM_MAX_NAME_LENGTH (32)     /*!< Max Module Name Length */
#define MODEM_MAX_OPERATOR_LENGTH (32) /*!< Max Operator Name Length */
#define MODEM_IMEI_LENGTH (15)         /*!< IMEI Number Length */
#define MODEM_IMSI_LENGTH (15)         /*!< IMSI Number Length */

#define esp_modem_dte_init esp_modem_dte_new

typedef esp_modem_dte_t modem_dte_t;

typedef struct modem_dce modem_dce_t;

struct modem_dce {
    char imei[MODEM_IMEI_LENGTH + 1];                                                 /*!< IMEI number */
    char imsi[MODEM_IMSI_LENGTH + 1];                                                 /*!< IMSI number */
    char name[MODEM_MAX_NAME_LENGTH];                                                 /*!< Module name */
    char oper[MODEM_MAX_OPERATOR_LENGTH];                                             /*!< Operator name */
    esp_modem_state_t state;                                                              /*!< Modem working state */
    esp_modem_mode_t mode;                                                                /*!< Working mode */
    modem_dte_t *dte;                                                                 /*!< DTE which connect to DCE */
    esp_err_t (*handle_line)(modem_dce_t *dce, const char *line);                     /*!< Handle line strategy */
    esp_err_t (*sync)(modem_dce_t *dce);                                              /*!< Synchronization */
    esp_err_t (*echo_mode)(modem_dce_t *dce, bool on);                                /*!< Echo command on or off */
    esp_err_t (*store_profile)(modem_dce_t *dce);                                     /*!< Store user settings */
    esp_err_t (*set_flow_ctrl)(modem_dce_t *dce, esp_modem_flow_ctrl_t flow_ctrl);        /*!< Flow control on or off */
    esp_err_t (*get_signal_quality)(modem_dce_t *dce, uint32_t *rssi, uint32_t *ber); /*!< Get signal quality */
    esp_err_t (*get_battery_status)(modem_dce_t *dce, uint32_t *bcs,
                                    uint32_t *bcl, uint32_t *voltage);  /*!< Get battery status */
    esp_err_t (*define_pdp_context)(modem_dce_t *dce, uint32_t cid,
                                    const char *type, const char *apn); /*!< Set PDP Contex */
    esp_err_t (*set_working_mode)(modem_dce_t *dce, esp_modem_mode_t mode); /*!< Set working mode */
    esp_err_t (*hang_up)(modem_dce_t *dce);                             /*!< Hang up */
    esp_err_t (*power_down)(modem_dce_t *dce);                          /*!< Normal power down */
    esp_err_t (*deinit)(modem_dce_t *dce);                              /*!< Deinitialize */
    esp_modem_dce_t parent;
};

modem_dce_t *sim800_init(modem_dte_t *dte);
modem_dce_t *sim7600_init(modem_dte_t *dte);
modem_dce_t *bg96_init(modem_dte_t *dte);

#ifdef __cplusplus
}
#endif
