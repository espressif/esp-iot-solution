idf_component_register(SRCS "app_main.c" "ble_jsonl.c" "indicator.c"
                       INCLUDE_DIRS "."
                       REQUIRES ble_uart nvs_flash esp_driver_gpio esp_timer freertos)
