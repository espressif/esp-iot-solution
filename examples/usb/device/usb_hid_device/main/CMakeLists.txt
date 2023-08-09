idf_component_register(
    SRCS "../hid_device/usb_descriptors.c" "tinyusb_hid.c" "usb_hid_device_main.c"
    INCLUDE_DIRS "../hid_device"
)

if("${IDF_VERSION_MAJOR}.${IDF_VERSION_MINOR}" VERSION_GREATER_EQUAL "5.0")
idf_component_get_property(tusb_lib espressif__tinyusb COMPONENT_LIB)
else()
idf_component_get_property(tusb_lib leeebo__tinyusb_src COMPONENT_LIB)
endif()

cmake_policy(SET CMP0079 NEW)
target_link_libraries(${tusb_lib} PRIVATE ${COMPONENT_LIB})
