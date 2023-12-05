# The following lines of boilerplate have to be in your project's CMakeLists
# in this exact order for cmake to work correctly
cmake_minimum_required(VERSION 3.5)

set(EXTRA_COMPONENT_DIRS "$ENV{IDF_PATH}/tools/unit-test-app/components"
                         "../../../esp_sensorless_bldc_control")
include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(bldc_step_by_step)
