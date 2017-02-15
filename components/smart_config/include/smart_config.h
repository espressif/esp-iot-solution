#ifndef _IOT_SMARTCONFIG_H_
#define _IOT_SMARTCONFIG_H_

#include "esp_smartconfig.h"
#include "esp_wifi.h"

/**
 * @brief Get smartconfig status
 * @return Smartconfig status
 */
smartconfig_status_t sc_get_status();

/**
  * @brief  SmartConfig initialize.
  * @param sc_type smartconfig protocol type
  * @param wifi_mode 
  * @return
  *     - ESP_OK: succeed
  *     - others: fail
  */
esp_err_t sc_setup(smartconfig_type_t sc_type, wifi_mode_t wifi_mode, bool fast_mode_en);

/**
  * @brief  Start SmartConfig .
  * @param  ticks_to_wait system tick number to wait
  * @return
  *     - ESP_OK: succeed
  *     - ESP_FAIL: fail
  *     - ESP_TIMEOUT: timeout
  */
esp_err_t sc_start(uint32_t ticks_to_wait);

/**
 * @brief Smartconfig stop
 */
void smartconfig_stop();

#endif
