set(srcs "app_hid.c" "app_uac.c" "app_usb.c" "app_vendor.c"
         "usb_extend_screen.c" "usb_frame.c")

if(CONFIG_HID_TOUCH_ENABLE)
    list (APPEND srcs "app_touch.c")
endif()

if(CONFIG_UAC_AUDIO_ENABLE)
    list (APPEND srcs "app_uac.c")
endif()


if(CONFIG_IDF_TARGET_ESP32P4)
    list(APPEND srcs "app_lcd_p4.c")
elseif(CONFIG_IDF_TARGET_ESP32S3)
    list(APPEND srcs "app_lcd_s3.c")
endif()

idf_component_register(SRCS ${srcs}
                       INCLUDE_DIRS "include" "tusb")

idf_component_get_property(UAC_PATH usb_device_uac COMPONENT_DIR)

# Determine whether tinyusb is fetched from component registry or from local path
idf_build_get_property(build_components BUILD_COMPONENTS)
if(tinyusb IN_LIST build_components)
    set(tinyusb_name tinyusb) # Local component
else()
    set(tinyusb_name espressif__tinyusb) # Managed component
endif()

idf_component_get_property(tusb_lib ${tinyusb_name} COMPONENT_LIB)
target_include_directories(${tusb_lib} PUBLIC "${PROJECT_DIR}/main/tusb" "${UAC_PATH}/tusb_uac")
target_sources(${tusb_lib} PUBLIC "${PROJECT_DIR}/main/tusb/usb_descriptors.c")

cmake_policy(SET CMP0079 NEW)
target_link_libraries(${tusb_lib} PRIVATE ${COMPONENT_LIB})
