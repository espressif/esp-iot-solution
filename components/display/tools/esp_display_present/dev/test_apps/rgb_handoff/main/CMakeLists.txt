# SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
# SPDX-License-Identifier: Apache-2.0
set(PRESENT_BOARD "$ENV{PRESENT_TEST_PRESENT_BOARD}")
if(PRESENT_BOARD STREQUAL "s31-rgb" AND IDF_TARGET STREQUAL "esp32s31")
    set(BOARD_SOURCE ../../boards/main/board_s31_rgb.c)
    set(BOARD_DEPS "")
elseif(PRESENT_BOARD STREQUAL "s3-rgb" AND IDF_TARGET STREQUAL "esp32s3")
    set(BOARD_SOURCE ../../boards/main/board_s3.c)
    set(BOARD_DEPS esp_lcd_gc9503 esp_lcd_panel_io_additions esp_io_expander_tca9554 esp_driver_i2c)
else()
    message(FATAL_ERROR "RGB handoff test requires confirmed S31 Korvo or S3 R16")
endif()
idf_component_register(SRCS main.c ../../boards/main/board_rgb_info.c ${BOARD_SOURCE} PRIV_INCLUDE_DIRS ../../boards/main
    REQUIRES esp_display_present esp_lcd esp_timer esp_psram esp_driver_gpio ${BOARD_DEPS})
if(PRESENT_BOARD STREQUAL "s31-rgb")
    target_compile_definitions(${COMPONENT_LIB} PRIVATE PRESENT_BOARD_S31_RGB=1 PRESENT_RGB_BOUNCE_LINES=0)
endif()

idf_component_get_property(_present_dir esp_display_present COMPONENT_DIR)
target_include_directories(${COMPONENT_LIB} PRIVATE "${_present_dir}/src")
target_compile_definitions(${COMPONENT_LIB} PRIVATE PRESENT_BOARD_RGB_INFO=1)
