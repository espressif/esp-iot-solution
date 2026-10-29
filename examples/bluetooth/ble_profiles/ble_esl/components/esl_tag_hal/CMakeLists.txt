set(srcs "esl_tag_hal_mock.c"
         "esl_tag_storage_nvs.c"
         "esl_tag_hal_display_epaper.c"
         "esl_tag_image_store.c"
         "esl_ble_compat.c")

idf_component_register(SRCS ${srcs}
                       INCLUDE_DIRS "include"
                       REQUIRES esl_product_common ble_esl log esp_driver_gpio led_strip epaper
                                esp_partition nvs_flash)
