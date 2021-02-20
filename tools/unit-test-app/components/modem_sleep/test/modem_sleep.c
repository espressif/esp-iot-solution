/* Wifi modem sleep mode and normal mode test
    
    start wifi and connet to a router, it will enter modem-sleep mode 10 seconds later if power-save mode are setted, 
    
    Do not forget change  DEFAULT_SSID & DEFAULT_PWD to do this test case.
*/
#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "esp_wifi.h"
#include "esp_wifi_types.h"
#include "esp_log.h"
#include "esp_event_loop.h"
#include "nvs_flash.h"
#include "unity.h"
#include "esp_system.h"

#define TEST_LOG(msg) printf("\n\n>>>   %s   <<<\n\n",msg)


/* set the ssid and password via "make menuconfig" under the directory unit-test-app */
#define WIFI_SSID   CONFIG_AP_SSID
#define WIFI_PWD    CONFIG_AP_PASSWORD


#define WIFI_STATUS_STOP           0x0
#define WIFI_STATUS_NORMAL         0x1
#define WIFI_STATUS_MORDEM_SLEEP   0x2


static const char *TAG = "power_save";


static uint32_t  wifi_status = WIFI_STATUS_STOP;


static esp_err_t event_handler(void *ctx, system_event_t *event)
{
    switch(event->event_id) {
    case SYSTEM_EVENT_STA_START:
	ESP_LOGI(TAG, "SYSTEM_EVENT_STA_START");
	ESP_ERROR_CHECK(esp_wifi_connect());
	break;
    case SYSTEM_EVENT_STA_GOT_IP:
	ESP_LOGI(TAG, "SYSTEM_EVENT_STA_GOT_IP");
	ESP_LOGI(TAG, "got ip:%s\n",
		ip4addr_ntoa(&event->event_info.got_ip.ip_info.ip));
        break;
    case SYSTEM_EVENT_STA_DISCONNECTED:
	ESP_LOGI(TAG, "SYSTEM_EVENT_STA_DISCONNECTED");
	break;
    default:
        break;
    }
    return ESP_OK;
}

/*init wifi as sta and set normal mode*/
static void wifi_start(void)
{
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }

    ESP_ERROR_CHECK( ret );
    tcpip_adapter_init();
    esp_event_loop_init(event_handler, NULL);
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));
    wifi_config_t wifi_config = {
	    .sta = {
	        .ssid = WIFI_SSID,
	        .password = WIFI_PWD
	    },
    };
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(ESP_IF_WIFI_STA, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());
    esp_wifi_set_ps(WIFI_PS_NONE);
}

/* disconnect router, stop wifi and deinit wifi */
static void wifi_stop(void)
{   
    esp_wifi_disconnect();
    esp_wifi_stop();
    esp_wifi_deinit();
    wifi_status = WIFI_STATUS_STOP;
}

/* set power-save mode(modem-sleep mode). */
static void moden_sleep_start(void) 
{
    esp_wifi_set_ps(WIFI_PS_MIN_MODEM);   
}

/* exit modem-sleep mode and enter normal mode */
static void moden_sleep_stop(void) 
{
    esp_wifi_set_ps(WIFI_PS_NONE);
}


TEST_CASE("Wifi stop test", "[wifi_test][iot]")
{
    if(wifi_status != WIFI_STATUS_STOP) {
        wifi_stop();
        TEST_LOG("Wifi stoped");
    } else {
        TEST_LOG("Wifi not start yeat");
    }
}


TEST_CASE("Connect router test", "[wifi_test][iot]")
{
    if(wifi_status == WIFI_STATUS_STOP) {
        wifi_status = WIFI_STATUS_NORMAL;
        wifi_start();
        TEST_LOG("Wifi started");
    } else {
        TEST_LOG("Wifi connected to a router aleardy");
        esp_wifi_set_mode(WIFI_MODE_STA);
    }
}


TEST_CASE("Modem sleep start test", "[mordem_sleep][iot]")
{
    if(wifi_status == WIFI_STATUS_STOP) {
        TEST_LOG("please do \"Connect router test\" first");
    } else if(wifi_status == WIFI_STATUS_NORMAL) {
        wifi_status = WIFI_STATUS_MORDEM_SLEEP;
        moden_sleep_start();
        TEST_LOG("Enter modem sleep mode");
    } else {
        TEST_LOG("Aleardy in mordem sleep mode");    
    }
}


TEST_CASE("Modem sleep stop test", "[mordem_sleep][iot]")
{
    if(wifi_status == WIFI_STATUS_STOP) {
        TEST_LOG("please do \"Connect router test\" first");
    } else if(wifi_status == WIFI_STATUS_MORDEM_SLEEP) {
        wifi_status = WIFI_STATUS_NORMAL;
        moden_sleep_stop();
        TEST_LOG("Exit modem sleep mode");
    } else {
        TEST_LOG("Aleardy exit mordem sleep mode");    
    }    
}
