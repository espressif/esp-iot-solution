cmake_minimum_required(VERSION 3.16)

set(EXTRA_COMPONENT_DIRS "../..")
include($ENV{IDF_PATH}/tools/cmake/project.cmake)
idf_build_set_property(MINIMAL_BUILD ON)
project(esp_lcd_touch_mux_test)
