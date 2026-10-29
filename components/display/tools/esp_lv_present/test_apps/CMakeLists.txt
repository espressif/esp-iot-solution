# SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
#
# SPDX-License-Identifier: Apache-2.0

cmake_minimum_required(VERSION 3.5)
set(_unit_test_components "$ENV{IDF_PATH}/tools/unit-test-app/components")
if(EXISTS "${_unit_test_components}")
    set(EXTRA_COMPONENT_DIRS "${_unit_test_components}")
endif()
include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(test_esp_lv_present)
