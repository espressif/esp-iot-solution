cmake_minimum_required(VERSION 3.16)

set(EXTRA_COMPONENT_DIRS "../../../components/esp_gmp"
                          "../../../components/esp_flux")

include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(esp_gmp_test)
