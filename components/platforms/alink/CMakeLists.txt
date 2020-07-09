

set(COMPONENT_SRCS  "adaptation/alink_network.c"
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


# requirements can't depend on config
set(COMPONENT_REQUIRES esp32 app_update json openssl nvs_flash)

register_component()

if(CONFIG_ALINK_VERSION_EMBED)
target_link_libraries(${COMPONENT_TARGET} "-L${COMPONENT_PATH}/esp32-alink/lib")
target_link_libraries(${COMPONENT_TARGET} alink_agent tfspal)
endif()
if(CONFIG_ALINK_VERSION_SDS)
target_link_libraries(${COMPONENT_TARGET} "-L${COMPONENT_PATH}/esp32-alink/lib")
target_link_libraries(${COMPONENT_TARGET} alink_agent_sds tfspal)
endif()
