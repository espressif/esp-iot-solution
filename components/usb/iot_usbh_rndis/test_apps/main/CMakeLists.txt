idf_component_register(SRC_DIRS "."
                        INCLUDE_DIRS "."
                        PRIV_REQUIRES unity iot_eth iot_usbh_rndis esp_netif lwip
                        )

target_link_libraries(${COMPONENT_LIB} INTERFACE "-u TEST_MEMORY_LEAK_THRESHOLD")
