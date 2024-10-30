idf_component_register(SRCS "usb_device_cdc.c" "usb_host_msc.c" "manual_switch.c" "../cdc_device/usb_descriptors.c"
                    INCLUDE_DIRS "include" "../cdc_device")

idf_component_get_property(tusb_lib espressif__tinyusb COMPONENT_LIB)

cmake_policy(SET CMP0079 NEW)
target_link_libraries(${tusb_lib} PRIVATE ${COMPONENT_LIB})
