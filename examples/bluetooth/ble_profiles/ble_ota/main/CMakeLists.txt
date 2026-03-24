idf_component_register(SRCS "app_main.c" "ble_ota_raw.c"
                       INCLUDE_DIRS "include"
                       PRIV_REQUIRES app_update nvs_flash esp_ringbuf bt)
