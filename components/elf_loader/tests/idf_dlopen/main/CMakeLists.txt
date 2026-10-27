# SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
# SPDX-License-Identifier: Apache-2.0
idf_component_register(SRCS "test_dlopen.c" REQUIRES elf_loader unity spiffs)

# Build the repository's actual project_so() fixture. The standalone
# shared-library project must run its so build target, not a firmware build.
include(ExternalProject)
idf_build_get_property(idf_path IDF_PATH)
idf_build_get_property(idf_target IDF_TARGET)

set(so_path "${CMAKE_CURRENT_LIST_DIR}/../../../../../examples/elf_loader/build_shared_library_example")
set(so_build "${CMAKE_CURRENT_BINARY_DIR}/shared_library_build")
set(so_file "${so_build}/lib.so")
set(so_payload "${CMAKE_CURRENT_BINARY_DIR}/lib_dlopen_payload.bin")

ExternalProject_Add(elf_dlopen_so
    SOURCE_DIR "${so_path}"
    BINARY_DIR "${so_build}"
    CMAKE_ARGS
        -DIDF_PATH=${idf_path}
        -DIDF_TARGET=${idf_target}
        -DSDKCONFIG=${so_build}/sdkconfig
    CMAKE_GENERATOR "Unix Makefiles"
    BUILD_COMMAND make so
    BUILD_ALWAYS 1
    BUILD_BYPRODUCTS "${so_file}"
    INSTALL_COMMAND ""
)
add_custom_command(
    OUTPUT "${so_payload}"
    COMMAND "${CMAKE_COMMAND}" -E copy "${so_file}" "${so_payload}"
    DEPENDS elf_dlopen_so "${so_file}"
    VERBATIM
)
add_custom_target(elf_dlopen_payload ALL DEPENDS "${so_payload}")
add_dependencies(${COMPONENT_LIB} elf_dlopen_payload)
elf_embed_aligned_binary(${COMPONENT_LIB} "${so_payload}" BINARY)
