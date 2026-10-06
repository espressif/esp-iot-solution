# SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
#
# SPDX-License-Identifier: Apache-2.0

idf_component_register(
    SRCS "test_esp_lv_present.c"
    INCLUDE_DIRS "."
    REQUIRES unity esp_lv_present
    PRIV_REQUIRES esp_lcd esp_timer
)

idf_component_get_property(esp_lcd_dir esp_lcd COMPONENT_DIR)
target_include_directories(${COMPONENT_LIB} PRIVATE "${esp_lcd_dir}/interface")
