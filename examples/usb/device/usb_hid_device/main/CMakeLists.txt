idf_component_register(
    SRCS "tinyusb_hid.c" "usb_hid_device_main.c"
    INCLUDE_DIRS "."
    PRIV_INCLUDE_DIRS "../hid_device"
)

# Determine whether tinyusb is fetched from component registry or from local path
idf_build_get_property(build_components BUILD_COMPONENTS)
if(tinyusb IN_LIST build_components)
    set(tinyusb_name tinyusb) # Local component
else()
    set(tinyusb_name espressif__tinyusb) # Managed component
endif()

# Pass tusb_config.h to the example
idf_component_get_property(tusb_lib ${tinyusb_name} COMPONENT_LIB)
target_include_directories(${tusb_lib} PUBLIC "../hid_device")
target_sources(${tusb_lib} PUBLIC "../hid_device/usb_descriptors.c")

cmake_policy(SET CMP0079 NEW)
