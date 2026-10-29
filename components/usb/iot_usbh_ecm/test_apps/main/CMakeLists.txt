idf_component_register(SRC_DIRS "."
                        INCLUDE_DIRS "."
                        PRIV_REQUIRES unity test_utils iot_eth iot_usbh_ecm
                        )

target_link_libraries(${COMPONENT_LIB} INTERFACE "-u TEST_MEMORY_LEAK_THRESHOLD")
