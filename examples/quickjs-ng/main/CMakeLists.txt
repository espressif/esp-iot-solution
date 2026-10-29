# SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
# SPDX-License-Identifier: Apache-2.0
#
# Embeds hello_world.js (EMBED_TXTFILES) as rodata: symbols
# _binary_hello_world_js_start / _binary_hello_world_js_end.

idf_component_register(
    SRCS
        "qjs_main.c"
    INCLUDE_DIRS
        "."
    EMBED_TXTFILES
        "hello_world.js"
)
