# The following lines of boilerplate have to be in your project's
# CMakeLists in this exact order for cmake to work correctly
cmake_minimum_required(VERSION 3.16)

# esp_xmodem is resolved by the IDF component manager via
# override_path declared in main/idf_component.yml - no EXTRA_COMPONENT_DIRS needed.

# A target-specific sdkconfig.defaults file (sdkconfig.defaults.<target>)
# must exist for each supported target. Use 'idf.py set-target <target>'
# to select one before building.
if(DEFINED IDF_TARGET AND
   EXISTS "${CMAKE_CURRENT_LIST_DIR}/sdkconfig.defaults.${IDF_TARGET}")
    set(SDKCONFIG_DEFAULTS "${CMAKE_CURRENT_LIST_DIR}/sdkconfig.defaults.${IDF_TARGET}")
endif()

include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(xmodem_receiver)
