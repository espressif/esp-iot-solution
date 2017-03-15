
#include "string.h"

#include "esp_wifi.h"
#include "esp_wifi_types.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "sys/socket.h"
#include "netdb.h"

#include "jd_innet.h"
#include "joylink_innet_ctl.h"
#include "joylink_smnt.h"
#include "joylink_smnt_adp.h"
#include "thunderconfig.h"
#include "thunderconfig_types.h"
#include "thunderconfig_utils.h"

#include "auth/aes.h"

#include "joylink_log.h"
#include "nvs.h"
#include "nvs_flash.h"

static char g_innet_aes_key[64];
const uint8_t AES_IV[16];
uint8_t jd_aes_out_buffer[128];
static xTaskHandle jd_innet_timer_task_handle = NULL;

void jd_innet_set_aes_key(char * pKey)
{
	if(pKey)
		memcpy(g_innet_aes_key, pKey, strlen(pKey)+1);
}

char * jd_innet_get_aes_key(void)
{
	return (char*)g_innet_aes_key;
}

/*recv packet callback*/
static void jd_innet_pack_callback(void *buf, wifi_promiscuous_pkt_type_t type)
{
    wifi_promiscuous_pkt_t *pack_all = NULL;
    int len=0;
    joylinkResult_t Ret;
    PHEADER_802_11 frame = NULL;
    uint8_t ssid_len = 0;

    if (type != WIFI_PKT_CTRL) {
        pack_all = (wifi_promiscuous_pkt_t *)buf;
        frame = (PHEADER_802_11)pack_all->payload;
        len = pack_all->rx_ctrl.sig_len;
        joylink_cfg_DataAction(frame, len);

        if (joylink_cfg_Result(&Ret) == 0) {
            if (Ret.type != 0) {
                memset(AES_IV,0x0,sizeof(AES_IV));
                len = device_aes_decrypt((const uint8 *)g_innet_aes_key, strlen(g_innet_aes_key),AES_IV,
                    Ret.encData + 1,Ret.encData[0],
                    jd_aes_out_buffer,sizeof(jd_aes_out_buffer));
                if (len > 0) {
                    wifi_config_t config;
                    memset(&config,0x0,sizeof(config));

                    if (jd_aes_out_buffer[0] > 32) {
                        log_debug("sta password len error\r\n");
                        return;
                    }
                    
                    memcpy(config.sta.password,jd_aes_out_buffer + 1,jd_aes_out_buffer[0]);

                    ssid_len = len - 1 - jd_aes_out_buffer[0] - 4 - 2;

                    if (ssid_len > sizeof(config.sta.ssid)) {
                        log_debug("sta ssid len error\r\n");
                        return;

                    }
                    strncpy((char*)config.sta.ssid,(const char*)(jd_aes_out_buffer + 1 + jd_aes_out_buffer[0] + 4 + 2),ssid_len);
                    log_debug("ssid:%s\r\n",config.sta.ssid);
                    log_debug("password:%s\r\n",config.sta.password);
                    if (esp_wifi_set_config(ESP_IF_WIFI_STA,&config) != ESP_OK) {
                        log_debug("set sta fail\r\n");
                    } else {
                        if (esp_wifi_connect() != ESP_OK) {
                            log_debug("sta connect fail\r\n");
                        } else {
                            if (jd_innet_timer_task_handle != NULL) {
                                vTaskDelete(jd_innet_timer_task_handle);
                                jd_innet_timer_task_handle = NULL;
                            }
                            esp_wifi_set_promiscuous(0);
                            // save flash
                            nvs_handle out_handle;
                            char data[65];
                            if (nvs_open("joylink_wifi", NVS_READWRITE, &out_handle) != ESP_OK) {
                                return;
                            }

                            memset(data,0x0,sizeof(data));
                            strncpy(data,(char*)config.sta.ssid,sizeof(config.sta.ssid));
                            if (nvs_set_str(out_handle,"ssid",data) != ESP_OK) {
                                log_debug("--set ssid fail");
                            }

                            memset(data,0x0,sizeof(data));
                            strncpy(data,(char*)config.sta.password,sizeof(config.sta.password));
                            if (nvs_set_str(out_handle,"password",data) != ESP_OK) {
                                log_debug("--set password fail");
                            }
                            nvs_close(out_handle);
                        }
                    }
                } else {
                    log_debug("aes fail\r\n");
                }
            }
        }
    }
}

void adp_changeCh(int i)
{
    esp_wifi_set_channel(i,WIFI_SECOND_CHAN_ABOVE);
}

static void jd_innet_timer_task (void *pvParameters) // if using timer,it will stack overflow because of log in joylink_cfg_50msTimer;
{
    int32_t delay_tick = 50;
    for (;;) {
        if (delay_tick == 0) {
            delay_tick = 1;
        }
        vTaskDelay(delay_tick);
        delay_tick = joylink_cfg_50msTimer()/portTICK_PERIOD_MS;
    }
}

static void jd_innet_start (void *pvParameters)
{
    unsigned char* pBuffer = NULL;
    
    esp_wifi_disconnect();
    esp_wifi_set_promiscuous(0);

    pBuffer = (unsigned char*)malloc(1024);
    if (pBuffer == NULL) {
        log_debug("%s,%d\r\n",__func__,__LINE__);
        vTaskDelete(NULL);
    }
    
    joylink_cfg_init(pBuffer);
    if (ESP_OK != esp_wifi_set_promiscuous_rx_cb(jd_innet_pack_callback)){
        log_debug ("[%s] set_promiscuous fail\n\r",__func__);
    }

    if (ESP_OK != esp_wifi_set_promiscuous(1)){
        log_debug ("[%s] open promiscuous fail\n\r",__func__);
    }

    if (jd_innet_timer_task_handle != NULL) {
        vTaskDelete(jd_innet_timer_task_handle);
        jd_innet_timer_task_handle = NULL;
    }
    xTaskCreate(jd_innet_timer_task, "jd_innet_timer_task", 2048, NULL, tskIDLE_PRIORITY + 5, &jd_innet_timer_task_handle);

    vTaskDelete(NULL);
}

void jd_innet_start_task(void)
{
    xTaskCreate(jd_innet_start, "jd_innet_start", 2048, NULL, tskIDLE_PRIORITY + 2, NULL);
}

