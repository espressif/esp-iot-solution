set(AP_SRCS "app_main.c"
            "ble_esl_ap.c"
            "app_esl_ap.c"
            "app_inventory.c"
            "app_recovery.c"
            "esl_ap_arbiter.c"
            "esl_ap_led_preset.c")

set(AP_REQUIRES nvs_flash esl_product_common esl_image_manager ble_esl ble_conn_mgr bt)

if(CONFIG_ESL_AP_MODE_WITH_APP)
    list(APPEND AP_SRCS "app_gateway.c")
else()
    list(APPEND AP_SRCS "app_gateway_stub.c")
endif()

idf_component_register(SRCS ${AP_SRCS}
                       INCLUDE_DIRS "."
                       REQUIRES ${AP_REQUIRES})
