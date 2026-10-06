# SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
#
# SPDX-License-Identifier: Apache-2.0

idf_component_register(
    SRCS "test_esp_display_present.c"
    INCLUDE_DIRS "."
    REQUIRES unity esp_display_present
    PRIV_REQUIRES esp_lcd esp_timer
)

idf_component_get_property(esp_lcd_dir esp_lcd COMPONENT_DIR)
target_include_directories(${COMPONENT_LIB} PRIVATE
    "${esp_lcd_dir}/interface"
    "${CMAKE_CURRENT_LIST_DIR}/../../private_include")

# The framebuffer contract tests provide a deterministic in-memory RGB panel.
# Wrap only callback registration; pixel submission still goes through the real
# presenter, mode, transform, repair and esp_lcd_panel_draw_bitmap paths.
if(CONFIG_IDF_TARGET_ESP32S3 OR CONFIG_IDF_TARGET_ESP32P4 OR CONFIG_IDF_TARGET_ESP32S31)
    target_include_directories(${COMPONENT_LIB} PRIVATE "${esp_lcd_dir}/rgb/include")
    target_link_options(${COMPONENT_LIB} INTERFACE
        "-Wl,--wrap=esp_lcd_rgb_panel_register_event_callbacks")
endif()
