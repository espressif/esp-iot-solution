idf_component_register(SRCS usb_device_uac.c
                    INCLUDE_DIRS "include"
                    PRIV_REQUIRES usb esp_timer)

idf_component_get_property(tusb_lib leeebo__tinyusb_src COMPONENT_LIB)

target_include_directories(${tusb_lib} PUBLIC "${COMPONENT_DIR}/tusb")
target_sources(${tusb_lib} PUBLIC "${COMPONENT_DIR}/tusb/usb_descriptors.c")

include(package_manager)
cu_pkg_define_version(${CMAKE_CURRENT_LIST_DIR})
