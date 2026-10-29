idf_component_register(SRCS "app_main.c"
                            "app_esl_tag.c"
                            "app_tag_events.c"
                       INCLUDE_DIRS "."
                       REQUIRES nvs_flash esl_product_common esl_tag_hal ble_esl ble_conn_mgr bt)
