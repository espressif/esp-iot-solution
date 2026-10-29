# SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
# SPDX-License-Identifier: Apache-2.0
#
# Example: QuickJS-NG on ESP-IDF. Depends on quickjs-ng through idf_component.yml.
# Build: export IDF_PATH, then from this directory: idf.py set-target <chip> build
# See: https://docs.espressif.com/projects/esp-idf/en/latest/api-guides/build-system.html

cmake_minimum_required(VERSION 3.16)

set(COMPONENTS main)

include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(quickjs_ng)
