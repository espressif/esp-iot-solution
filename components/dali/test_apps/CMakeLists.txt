
# The following lines of boilerplate have to be in your project's CMakeLists
# in this exact order for cmake to work correctly
cmake_minimum_required(VERSION 3.5)

set(SUPPORTED_TARGETS esp32 esp32s2 esp32s3 esp32c3 esp32c6 esp32h2 esp32p4)
set(EXTRA_COMPONENT_DIRS "../../dali")
include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(dali_test)
