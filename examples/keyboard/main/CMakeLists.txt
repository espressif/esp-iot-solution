idf_build_get_property(target IDF_TARGET)

set(_kbd_srcs
    "main.c"
    "../hid_device/usb_descriptors.c"
    "tinyusb_hid.c"
    "ble_hid.c"
    "esp_hid_gap.c"
    "btn_progress.c"
    "settings.c"
    "hid_transport.c"
    "hw_mode_switch.c"
)

if(CONFIG_KEYBOARD_BATTERY_ADC_ENABLE)
    list(APPEND _kbd_srcs "battery_adc.c")
endif()

if("${target}" STREQUAL "esp32h4")
    set(_kbd_bsp esp32_h4_kbd_kit)
elseif("${target}" STREQUAL "esp32s3")
    set(_kbd_bsp esp32_s3_kbd_kit)
else()
    message(FATAL_ERROR "keyboard example supports ESP32-S3 and ESP32-H4 only (current: ${target})")
endif()

if(${IDF_VERSION_MAJOR} LESS 6)
    set(_kbd_priv_requires usb)       # esp_private/usb_phy.h is in usb component on IDF < 6.0
else()
    set(_kbd_priv_requires esp_hw_support)
endif()

set(_kbd_requires
    ${_kbd_bsp}
    keyboard_button
    espressif__led_strip
    lamp_array_matrix
    lijunru-hub__keyboard_rgb_matrix
    esp_hid
    bt
    nvs_flash
    esp_timer
    espressif__tinyusb
)

if(CONFIG_KEYBOARD_BATTERY_ADC_ENABLE)
    list(APPEND _kbd_requires espressif__adc_battery_estimation)
endif()

idf_component_register(SRCS ${_kbd_srcs}
                       INCLUDE_DIRS "../hid_device" "../common"
                       REQUIRES ${_kbd_requires}
                       PRIV_REQUIRES ${_kbd_priv_requires})

idf_component_get_property(tusb_lib espressif__tinyusb COMPONENT_LIB)

cmake_policy(SET CMP0079 NEW)
target_link_libraries(${tusb_lib} PRIVATE ${COMPONENT_LIB})
