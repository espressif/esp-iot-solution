# SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
# SPDX-License-Identifier: Apache-2.0
#
# Applications include only quickjs.h / quickjs-libc.h from INCLUDE_DIRS; port/include is
# for compiling this component only (poll.h, esp_libc_compat, etc.).

idf_component_register(
    SRCS
        "quickjs-ng/dtoa.c"
        "quickjs-ng/libregexp.c"
        "quickjs-ng/libunicode.c"
        "quickjs-ng/quickjs.c"
        "quickjs-ng/quickjs-libc.c"
        "port/stub.c"
    INCLUDE_DIRS
        "quickjs-ng"
    PRIV_INCLUDE_DIRS
        "port/include"
    PRIV_REQUIRES
        vfs
)

target_compile_definitions(${COMPONENT_LIB} PRIVATE
    QUICKJS_NG_BUILD
    _GNU_SOURCE
)

# Third-party source files: suppress warnings that are noisy but harmless in upstream code.
set(_qjs_vendor_warnings
    -Wno-pointer-sign
    -Wno-incompatible-pointer-types
    -Wno-cast-function-type
    -Wno-unused-parameter
    -Wno-unused-variable
    -Wno-unused-but-set-variable
    -Wno-sign-compare
    -Wno-missing-field-initializers
    -Wno-type-limits
    -Wno-implicit-fallthrough
    -Wno-format
    -Wno-strict-aliasing
    -Wno-empty-body
)

# Apply vendor warnings only to upstream QuickJS files (not port/stub.c)
set_source_files_properties(
    "${CMAKE_CURRENT_SOURCE_DIR}/quickjs-ng/dtoa.c"
    PROPERTIES COMPILE_OPTIONS "${_qjs_vendor_warnings};-Wno-uninitialized;-Wno-maybe-uninitialized"
)
set_source_files_properties(
    "${CMAKE_CURRENT_SOURCE_DIR}/quickjs-ng/libregexp.c"
    PROPERTIES COMPILE_OPTIONS "${_qjs_vendor_warnings}"
)
set_source_files_properties(
    "${CMAKE_CURRENT_SOURCE_DIR}/quickjs-ng/libunicode.c"
    PROPERTIES COMPILE_OPTIONS "${_qjs_vendor_warnings}"
)
set_source_files_properties(
    "${CMAKE_CURRENT_SOURCE_DIR}/quickjs-ng/quickjs.c"
    PROPERTIES COMPILE_OPTIONS "${_qjs_vendor_warnings};-Wno-maybe-uninitialized"
)

# quickjs-libc.c: ESP_PLATFORM feature gates for unsupported host APIs; esp_libc_compat.h for lstat and newlib.
set(_qjs_libc "${CMAKE_CURRENT_SOURCE_DIR}/quickjs-ng/quickjs-libc.c")
set(_qjs_libc_compat "${CMAKE_CURRENT_SOURCE_DIR}/port/include/esp_libc_compat.h")
set_source_files_properties(${_qjs_libc}
    PROPERTIES
        COMPILE_OPTIONS "-include;${_qjs_libc_compat};${_qjs_vendor_warnings}"
)

# newlib: poll and related declarations (quickjs-libc.c includes <poll.h> via port/include/poll.h).
idf_build_get_property(_qjs_idf_path IDF_PATH)
target_include_directories(${COMPONENT_LIB} SYSTEM PRIVATE
    "${_qjs_idf_path}/components/newlib/platform_include"
)
