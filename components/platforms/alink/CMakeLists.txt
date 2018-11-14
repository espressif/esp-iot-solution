
# componet standalone mode
if(NOT CONFIG_IOT_SOLUTION_EMBED)
    set(COMPONENT_SRCS "adaptation/alink_network.c"
                        "adaptation/alink_os.c"
                        "adaptation/alink_product.c"
                        "adaptation/alink_ssl.c"
                        "adaptation/alink_upgrade.c"
                        "adaptation/alink_wifi.c"

                        "esp32-alink/application/esp_alink_main.c"
                        "esp32-alink/application/esp_data_transport.c"
                        "esp32-alink/application/esp_info_store.c"
                        "esp32-alink/application/esp_json_parser.c")

    set(COMPONENT_ADD_INCLUDEDIRS "esp32-alink/include"
                                    "esp32-alink/adaptation/include")
else()
    if(CONFIG_IOT_ALINK_ENABLE)
        set(COMPONENT_SRCS "adaptation/alink_network.c"
                            "adaptation/alink_os.c"
                            "adaptation/alink_product.c"
                            "adaptation/alink_ssl.c"
                            "adaptation/alink_upgrade.c"
                            "adaptation/alink_wifi.c"

                            "esp32-alink/application/esp_alink_main.c"
                            "esp32-alink/application/esp_data_transport.c"
                            "esp32-alink/application/esp_info_store.c"
                            "esp32-alink/application/esp_json_parser.c")

        set(COMPONENT_ADD_INCLUDEDIRS "esp32-alink/include"
                                        "esp32-alink/adaptation/include")
    else()
        set(COMPONENT_SRCS "")
        set(COMPONENT_ADD_INCLUDEDIRS "")
        message(STATUS "Building empty esp32-alink component due to configuration")
    endif()
endif()

# requirements can't depend on config
set(COMPONENT_REQUIRES esp32 app_update json openssl nvs_flash)

register_component()

if(CONFIG_ALINK_VERSION_EMBED)
target_link_libraries(alink "-L${COMPONENT_PATH}/esp32-alink/lib")
target_link_libraries(alink alink_agent tfspal)
endif()
if(CONFIG_ALINK_VERSION_SDS)
target_link_libraries(alink "-L${COMPONENT_PATH}/esp32-alink/lib")
target_link_libraries(alink alink_agent_sds tfspal)
endif()
