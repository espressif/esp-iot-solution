#ifndef _IOT_PARAM_H_
#define _IOT_PARAM_H_

/**
  * @brief  save param to flash with protect
  *
  * @param  start_sec the first sector number of three sectors
  * @param  param the pointer to the param to be save
  * @param  len length of param,must be 4-byte aligned
  *
  * @return
  *     - ESP_OK: succeed
  *     - others: fail
  */
esp_err_t param_save(uint16_t start_sec, void *param, uint16_t len);

/**
  * @brief  read param from flash
  *
  * @param  start_sec the first sector number of three sectors
  * @param  dest the address to save read param
  * @param  len length of param,must be 4-byte aligned
  *
  * @return
  *     - ESP_OK: succeed
  *     - others: fail
  */
esp_err_t param_load(uint16_t start_sec, void *dest, uint16_t len);

#endif