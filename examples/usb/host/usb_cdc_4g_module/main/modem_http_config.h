#pragma once

#include "esp_err.h"
#include "esp_wifi.h"

#ifdef __cplusplus
extern "C"
{
#endif

typedef struct {
    uint8_t mac[6];
    char name[32];
    esp_ip4_addr_t ip;
    int64_t start_time;
}modem_netif_sta_info_t;
typedef struct modem_http_sta_list_t {
    modem_netif_sta_info_t *sta;
    SLIST_ENTRY(modem_http_sta_list_t) next;
}modem_http_sta_list_t;

esp_err_t modem_http_sta_list_updata(esp_netif_sta_list_t *netif_sta_list);
void modem_http_config_init(void);

#ifdef __cplusplus
}
#endif