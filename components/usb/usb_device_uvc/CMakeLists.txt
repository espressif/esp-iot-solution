idf_component_register(SRCS usb_device_uvc.c usb_descriptors.c
                    INCLUDE_DIRS "include"
                    PRIV_INCLUDE_DIRS "private_include"
                    REQUIRES usb esp_timer)

if("${IDF_VERSION_MAJOR}.${IDF_VERSION_MINOR}" VERSION_GREATER_EQUAL "5.0")
idf_component_get_property(tusb_lib espressif__tinyusb COMPONENT_LIB)
else()
idf_component_get_property(tusb_lib leeebo__tinyusb_src COMPONENT_LIB)
endif()

cmake_policy(SET CMP0079 NEW)
target_link_libraries(${tusb_lib} PRIVATE ${COMPONENT_LIB})

include(package_manager)
cu_pkg_define_version(${CMAKE_CURRENT_LIST_DIR})
