# SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
# SPDX-License-Identifier: Apache-2.0

# Emit aligned embedded data without relying on an IDF-specific ALIGN option.
# Symbol names and the TEXT terminator follow target_add_binary_data().
if(NOT DATA_FILE OR NOT SOURCE_FILE)
    message(FATAL_ERROR "DATA_FILE and SOURCE_FILE must be provided")
endif()

if(NOT FILE_TYPE STREQUAL "BINARY" AND NOT FILE_TYPE STREQUAL "TEXT")
    message(FATAL_ERROR "Unsupported embedded file type: ${FILE_TYPE}")
endif()

get_filename_component(varname "${DATA_FILE}" NAME)
string(MAKE_C_IDENTIFIER "${varname}" varname)
file(TO_CMAKE_PATH "${DATA_FILE}" data_path)
string(REPLACE "\\" "\\\\" data_path "${data_path}")
string(REPLACE "\"" "\\\"" data_path "${data_path}")

file(WRITE "${SOURCE_FILE}"
    "/* Generated aligned embedded data. */\n"
    ".data\n"
    "#if !defined (__APPLE__) && !defined (__linux__)\n"
    ".section .rodata.embedded\n"
    "#endif\n"
    ".balign 4\n"
    ".global ${varname}\n"
    "${varname}:\n"
    ".global _binary_${varname}_start\n"
    "_binary_${varname}_start:\n"
    ".incbin \"${data_path}\"\n"
)

if(FILE_TYPE STREQUAL "TEXT")
    file(APPEND "${SOURCE_FILE}" ".byte 0\n")
    set(length_adjust "- 1")
else()
    set(length_adjust "")
endif()

file(APPEND "${SOURCE_FILE}"
    ".global _binary_${varname}_end\n"
    "_binary_${varname}_end:\n"
    ".balign 4\n"
    ".global ${varname}_length\n"
    "${varname}_length:\n"
    ".long _binary_${varname}_end - _binary_${varname}_start ${length_adjust}\n"
)
