#ifndef __WIFI_H__
#define __WIFI_H__

#define WIFI_CONNECTED_EVT	BIT0
#define WIFI_STOP_REQ_EVT   BIT1


typedef enum {
    WIFI_STATUS_STA_DISCONNECTED,
    WIFI_STATUS_STA_CONNECTING,
    WIFI_STATUS_STA_CONNECTED,
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
  *     - others: timeout
  */
esp_err_t wifi_connect_start(const char *ssid, const char *pwd, uint32_t ticks_to_wait);

/**
 *  @brief WiFi stop connecting
 */
void wifi_connect_stop();

#endif
