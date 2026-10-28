idf_component_register(SRCS "esl_nvs.c" "esl_image_codec.c"
                       INCLUDE_DIRS "include"
                       REQUIRES nvs_flash esp_rom ble_esl ble_conn_mgr)
