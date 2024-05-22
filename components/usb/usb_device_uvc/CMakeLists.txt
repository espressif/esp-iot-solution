idf_component_register(SRCS usb_device_uvc.c
                    INCLUDE_DIRS "include"
                    REQUIRES usb esp_timer)

if("${IDF_VERSION_MAJOR}.${IDF_VERSION_MINOR}" VERSION_GREATER_EQUAL "5.0")
idf_component_get_property(tusb_lib tinyusb COMPONENT_LIB)
else()
idf_component_get_property(tusb_lib leeebo__tinyusb_src COMPONENT_LIB)
endif()

target_include_directories(${tusb_lib} PUBLIC "${COMPONENT_DIR}/tusb")
target_sources(${tusb_lib} PUBLIC "${COMPONENT_DIR}/tusb/usb_descriptors.c")

include(package_manager)
cu_pkg_define_version(${CMAKE_CURRENT_LIST_DIR})
