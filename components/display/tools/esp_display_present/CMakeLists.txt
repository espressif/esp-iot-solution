# SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
# SPDX-License-Identifier: Apache-2.0
include(${CMAKE_CURRENT_LIST_DIR}/src/sources.cmake)
idf_component_register(SRCS ${PRESENT_FACADE_SOURCES} ${PRESENT_RESOURCE_SOURCES} ${PRESENT_TE_SYNC_SOURCES} ${PRESENT_ENGINE_SOURCES}
    INCLUDE_DIRS include
    PRIV_INCLUDE_DIRS src
    REQUIRES esp_common esp_lcd
    PRIV_REQUIRES freertos esp_timer heap esp_mm esp_hw_support esp_driver_ppa esp_driver_dma esp_system esp_driver_gpio)
target_compile_features(${COMPONENT_LIB} PRIVATE c_std_11)
