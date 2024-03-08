set(priv_req wifi_provisioning qrcode nvs_flash esp_event rmaker_common)

idf_component_register(SRCS "app_wifi.c" "app_http.c"
                    INCLUDE_DIRS "include"
                    REQUIRES protocol_examples_common esp-tls esp_http_client json
                    PRIV_REQUIRES ${priv_req})
