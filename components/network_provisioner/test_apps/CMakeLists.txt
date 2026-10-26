# The following lines of boilerplate have to be in your project's CMakeLists
# in this exact order for cmake to work correctly
cmake_minimum_required(VERSION 3.5)

include($ENV{IDF_PATH}/tools/cmake/version.cmake)

set(SUPPORTED_TARGETS esp32 esp32s3 esp32c2 esp32c3 esp32c5 esp32c6 esp32c61 esp32p4)

# IDF 6.0 moved unit-test helpers from tools/unit-test-app to tools/test_apps.
if("${IDF_VERSION_MAJOR}.${IDF_VERSION_MINOR}" VERSION_GREATER_EQUAL "6.0")
    set(idf_test_components "$ENV{IDF_PATH}/tools/test_apps/components")
else()
    set(idf_test_components "$ENV{IDF_PATH}/tools/unit-test-app/components")
endif()

set(EXTRA_COMPONENT_DIRS "${idf_test_components}"
                         "../../protocomm_ext"
                         "../../network_provisioner")

include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(network_provisioner_test)
