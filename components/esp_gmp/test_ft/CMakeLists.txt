# SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
# SPDX-License-Identifier: Apache-2.0

cmake_minimum_required(VERSION 3.16)

set(EXTRA_COMPONENT_DIRS "../../../components/esp_gmp"
                          "../../../components/esp_flux")

include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(esp_gmp_ft_test)
