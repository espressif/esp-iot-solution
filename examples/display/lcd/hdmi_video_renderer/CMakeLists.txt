# For more information about build system see
# https://docs.espressif.com/projects/esp-idf/en/latest/api-guides/build-system.html
# The following five lines of boilerplate have to be in your project's
# CMakeLists in this exact order for cmake to work correctly
cmake_minimum_required(VERSION 3.5)
include($ENV{IDF_PATH}/tools/cmake/project.cmake)
add_compile_options("-Wno-attributes")
project(mipi_dsi_avoid_tearing)

# Suppress unused-variable warnings from managed component without touching its sources.
idf_component_get_property(ESP32_P4_EV_BOARD_LIB espressif__esp32_p4_function_ev_board COMPONENT_LIB)
if(ESP32_P4_EV_BOARD_LIB)
    target_compile_options(${ESP32_P4_EV_BOARD_LIB} PRIVATE -Wno-unused-variable)
endif()
