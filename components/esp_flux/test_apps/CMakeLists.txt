cmake_minimum_required(VERSION 3.16)

set(EXTRA_COMPONENT_DIRS "$ENV{IDF_PATH}/tools/unit-test-app/components"
                          "../../../components/esp_flux")

include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(test_esp_flux)
