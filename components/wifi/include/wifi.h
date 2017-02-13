#ifndef __WIFI_H__
#define __WIFI_H__
#include "esp_log.h"

#define WIFI_CONNECTED_EVT	BIT0
#define WIFI_STOP_REQ_EVT   BIT1

#define err_assert(param)      \
	if((param) != ESP_OK){     \
		ESP_LOGE("error_assert", "ERROR!\n");   \
		return ESP_FAIL;       \
	}

#define pointer_assert(tag, param)	\
	if((param) == NULL){		\
		ESP_LOGE(tag, "%s:%d (%s) - the point is null\n", __FILE__, __LINE__, __FUNCTION__);	\
		return ESP_FAIL;	\
	}


#define res_assert(tag, res, ret) \
        if((res) == pdFALSE) { \
            ESP_LOGE(tag, "%s:%d (%s)Res pdFALSE",__FILE__, __LINE__, __FUNCTION__); \
            return ret; \
        }

typedef enum {
    WIFI_STATUS_STA_DISCONNECTED,           /**< ESP32 station disconnected */
    WIFI_STATUS_STA_CONNECTING,             /**< ESP32 station connecting */
    WIFI_STATUS_STA_CONNECTED,              /**< ESP32 station connected */
} wifi_sta_status_t;

/**
  * @brief  wifi initialize.
  *
  * @return
  *     - ESP_OK: succeed
  *     - others: fail
  */
esp_err_t wifi_setup(wifi_mode_t);

/**
  * @brief  wifi connect with timeout
  *
  * @param  ssid ssid of the target AP
  * @param  pwd password of the target AP
  * @param  ticks_to_wait system tick number to wait
  *
  * @return
  *     - ESP_OK: connect to AP successfully
  *     - ESP_TIMEOUT: timeout
  *     - ESP_FAIL: fail
  */
esp_err_t wifi_connect_start(const char *ssid, const char *pwd, uint32_t ticks_to_wait);

/**
 *  @brief WiFi stop connecting
 */
void wifi_connect_stop();

/**
  * @brief  get wifi status.
  *
  * @return status of the wifi station
  */
wifi_sta_status_t wifi_get_status();

#endif
