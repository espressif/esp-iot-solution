# For more information about build system see
# https://docs.espressif.com/projects/esp-idf/en/latest/api-guides/build-system.html
# The following five lines of boilerplate have to be in your project's
# CMakeLists in this exact order for cmake to work correctly
cmake_minimum_required(VERSION 3.16)

include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(keyboard)

# Make MAX brightness configurable
idf_component_get_property(keyboard_rgb_matrix lijunru-hub__keyboard_rgb_matrix COMPONENT_LIB)
target_compile_definitions(${keyboard_rgb_matrix} PUBLIC "-DRGB_MATRIX_MAXIMUM_BRIGHTNESS=100")
