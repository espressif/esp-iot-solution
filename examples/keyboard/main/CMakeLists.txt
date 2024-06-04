idf_component_register(SRCS "main.c" "../hid_device/usb_descriptors.c" "tinyusb_hid.c" "ble_hid.c" "esp_hid_gap.c" "btn_progress.c" "settings.c"
                    INCLUDE_DIRS "../hid_device")

idf_component_get_property(tusb_lib espressif__tinyusb COMPONENT_LIB)

cmake_policy(SET CMP0079 NEW)
target_link_libraries(${tusb_lib} PRIVATE ${COMPONENT_LIB})
