#include "esp_wifi.h"
#include "esp_err.h"
#include "esp_types.h"
#include "wpa/common.h"
#include "wpa/ieee802_11_defs.h"
#include "lwip/inet.h"
#include "esp_event_loop.h"
#include "esp_system.h"
#include "esp_log.h"

#include "platform.h"
#include "alink_export.h"
#include "esp_alink.h"

static platform_awss_recv_80211_frame_cb_t g_sniffer_cb = NULL;
static const char *TAG = "alink_wifi";

//一键配置超时时间, 建议超时时间1-3min, APP侧一键配置1min超时
int platform_awss_get_timeout_interval_ms(void)
{
    return 60 * 1000;
}

//一键配置每个信道停留时间, 建议200ms-400ms
int platform_awss_get_channelscan_interval_ms(void)
{
    return 200;
}

//wifi信道切换，信道1-13
void platform_awss_switch_channel(char primary_channel,
                                  char secondary_channel, char bssid[ETH_ALEN])
{
    ESP_ERROR_CHECK(esp_wifi_set_channel(primary_channel, secondary_channel));
    //ret = system(buf);
}

static void IRAM_ATTR  wifi_sniffer_cb_(void *recv_buf, wifi_promiscuous_pkt_type_t type) {
    char *buf = NULL;
    uint16_t len = 0;
    // if (type == WIFI_PKT_CTRL) return;
    wifi_promiscuous_pkt_t *sniffer = (wifi_promiscuous_pkt_t*)recv_buf;
    buf = (char *)sniffer->payload;
    len = sniffer->rx_ctrl.sig_len;
    g_sniffer_cb(buf, len, AWSS_LINK_TYPE_NONE, 1);
}

//进入monitor模式, 并做好一些准备工作，如
//设置wifi工作在默认信道6
//若是linux平台，初始化socket句柄，绑定网卡，准备收包
//若是rtos的平台，注册收包回调函数aws_80211_frame_handler()到系统接口
void platform_awss_open_monitor(_IN_ platform_awss_recv_80211_frame_cb_t cb)
{
    g_sniffer_cb = cb;
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_promiscuous(0));
    ESP_ERROR_CHECK(esp_wifi_set_promiscuous_rx_cb(wifi_sniffer_cb_));
    ESP_ERROR_CHECK(esp_wifi_set_promiscuous(1));
    ESP_ERROR_CHECK(esp_wifi_set_channel(6, 0));
}

//退出monitor模式，回到station模式, 其他资源回收
void platform_awss_close_monitor(void)
{
    ESP_ERROR_CHECK(esp_wifi_set_promiscuous(0));
    ESP_ERROR_CHECK(esp_wifi_set_promiscuous_rx_cb(NULL));
}


int platform_wifi_get_rssi_dbm(void)
{
    wifi_ap_record_t ap_infor;
    esp_wifi_sta_get_ap_info(&ap_infor);
    return ap_infor.rssi;
}

char *platform_wifi_get_mac(_OUT_ char mac_str[PLATFORM_MAC_LEN])
{
    ALINK_PARAM_CHECK(mac_str == NULL);
    uint8_t mac[6] = {0};
    // wifi_mode_t mode_backup;
    // esp_wifi_get_mode(&mode_backup);
    // esp_wifi_set_mode(WIFI_MODE_STA);
    ESP_ERROR_CHECK(esp_wifi_get_mac(ESP_IF_WIFI_STA, mac));
    snprintf(mac_str, PLATFORM_MAC_LEN, MACSTR, MAC2STR(mac));
    // esp_wifi_set_mode(mode_backup);
    return mac_str;
}

uint32_t platform_wifi_get_ip(_OUT_ char ip_str[PLATFORM_IP_LEN])
{
    ALINK_PARAM_CHECK(ip_str == NULL);
    tcpip_adapter_ip_info_t infor;
    tcpip_adapter_get_ip_info(TCPIP_ADAPTER_IF_STA, &infor);
    memcpy(ip_str, inet_ntoa(infor.ip.addr), PLATFORM_IP_LEN);
    return infor.ip.addr;
}

static alink_err_t sys_net_is_ready = ALINK_FALSE;
int platform_sys_net_is_ready(void)
{
    return sys_net_is_ready;
}

static SemaphoreHandle_t xSemConnet = NULL;
static alink_err_t event_handler(void *ctx, system_event_t *event)
{
    switch (event->event_id) {
    case SYSTEM_EVENT_STA_START:
        ALINK_LOGI("SYSTEM_EVENT_STA_START");
        ESP_ERROR_CHECK( esp_wifi_connect() );
        break;
    case SYSTEM_EVENT_STA_GOT_IP:
        sys_net_is_ready = ALINK_TRUE;
        ALINK_LOGI("SYSTEM_EVENT_STA_GOT_IP");
        xSemaphoreGive(xSemConnet);
        break;
    case SYSTEM_EVENT_STA_DISCONNECTED:
        ALINK_LOGI("SYSTEM_EVENT_STA_DISCONNECTED");
        sys_net_is_ready = ALINK_FALSE;
        ESP_ERROR_CHECK( esp_wifi_connect() );
        break;
    default:
        break;
    }
    return ALINK_OK;
}

alink_err_t alink_write_wifi_config(_IN_ const wifi_config_t *wifi_config);
int platform_awss_connect_ap(
    _IN_ uint32_t connection_timeout_ms,
    _IN_ char ssid[PLATFORM_MAX_SSID_LEN],
    _IN_ char passwd[PLATFORM_MAX_PASSWD_LEN],
    _IN_OPT_ enum AWSS_AUTH_TYPE auth,
    _IN_OPT_ enum AWSS_ENC_TYPE encry,
    _IN_OPT_ uint8_t bssid[ETH_ALEN],
    _IN_OPT_ uint8_t channel)
{
    wifi_config_t wifi_config;
    if (xSemConnet == NULL) {
        xSemConnet = xSemaphoreCreateBinary();
        esp_event_loop_set_cb(event_handler, NULL);
    }

    ESP_ERROR_CHECK( esp_wifi_get_config(WIFI_IF_STA, &wifi_config) );
    memcpy(wifi_config.sta.ssid, ssid, sizeof(wifi_config.sta.ssid));
    memcpy(wifi_config.sta.password, passwd, sizeof(wifi_config.sta.password));
    ALINK_LOGI("ap ssid: %s, password: %s", wifi_config.sta.ssid, wifi_config.sta.password);

    ESP_ERROR_CHECK( esp_wifi_set_mode(WIFI_MODE_STA) );
    ESP_ERROR_CHECK( esp_wifi_set_config(WIFI_IF_STA, &wifi_config) );
    ESP_ERROR_CHECK( esp_wifi_start() );

    BaseType_t err = xSemaphoreTake(xSemConnet, connection_timeout_ms / portTICK_RATE_MS);
    if (err != pdTRUE) ESP_ERROR_CHECK( esp_wifi_stop() );
    ALINK_ERROR_CHECK(err != pdTRUE, ALINK_ERR, "xSemaphoreTake ret:%x wait: %d", err, connection_timeout_ms);
    alink_write_wifi_config(&wifi_config);
    return ALINK_OK;
}


