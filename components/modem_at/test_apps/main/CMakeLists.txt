idf_component_register(SRC_DIRS "."
                        INCLUDE_DIRS "."
                        PRIV_REQUIRES unity test_utils modem_at driver iot_usbh_cdc usb
                        )

set_property(TARGET ${COMPONENT_LIB} APPEND PROPERTY INTERFACE_LINK_LIBRARIES
    "-u TEST_MEMORY_LEAK_THRESHOLD"
)
