#ifndef _IOT_PARAM_H_
#define _IOT_PARAM_H_
/**
  * @brief  save param to flash with protect
  *
  * @param  namespace
  * @param  key different param should have different key
  * @param  param
  * @param  len
  *
  * @return
  *     - ESP_OK: succeed
  *     - others: fail
  */
esp_err_t iot_param_save(const char* namespace, const char* key, void* param, uint16_t len);

/**
  * @brief  read param from flash
  *
  * @param  namespace
  * @param  key different param should have different key
  * @param  dest the address to save read param
  *
  * @return
  *     - ESP_OK: succeed
  *     - others: fail
  */
esp_err_t iot_param_load(const char* namespace, const char* key, void* dest);

#endif
