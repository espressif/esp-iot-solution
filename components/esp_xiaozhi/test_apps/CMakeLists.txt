# The following lines of boilerplate have to be in your project's CMakeLists
# in this exact order for cmake to work correctly
cmake_minimum_required(VERSION 3.5)

include($ENV{IDF_PATH}/tools/cmake/version.cmake)

if("${IDF_VERSION_MAJOR}.${IDF_VERSION_MINOR}" VERSION_GREATER_EQUAL "6.0")
    set(idf_test_components "$ENV{IDF_PATH}/tools/test_apps/components")
else()
    set(idf_test_components "$ENV{IDF_PATH}/tools/unit-test-app/components")
endif()

set(EXTRA_COMPONENT_DIRS "${idf_test_components}"
                         "$ENV{IDF_PATH}/examples/common_components/protocol_examples_common"
                         "../../esp_xiaozhi"
                         "../../mcp-c-sdk")

include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(esp_xiaozhi_test)
