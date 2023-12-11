idf_component_register( SRCS "usb_uac_device_main.c"
                        INCLUDE_DIRS "." "../uac_device")

if("${IDF_VERSION_MAJOR}.${IDF_VERSION_MINOR}" VERSION_GREATER_EQUAL "5.0")
idf_component_get_property(tusb_lib espressif__tinyusb COMPONENT_LIB)
else()
idf_component_get_property(tusb_lib leeebo__tinyusb_src COMPONENT_LIB)
endif()

cmake_policy(SET CMP0079 NEW)
target_include_directories(${tusb_lib} PRIVATE "${CMAKE_CURRENT_LIST_DIR}/../uac_device")
target_sources(${tusb_lib} PUBLIC "${CMAKE_CURRENT_LIST_DIR}/../uac_device/usb_descriptors.c" "${CMAKE_CURRENT_LIST_DIR}/tinyusb_uac.c")
include(cmake_utilities)
