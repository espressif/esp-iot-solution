# SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
# SPDX-License-Identifier: Apache-2.0
idf_component_register(SRCS "test_elf_runtime.c" REQUIRES elf_loader unity)

# The C++ fixture has no imported host symbols, so it does not need a
# generated symbol table. Build its project_elf() target for this chip and
# embed the output with the loader's cross-IDF alignment helper.
include(ExternalProject)
idf_build_get_property(idf_path IDF_PATH)
idf_build_get_property(idf_target IDF_TARGET)

set(module_path "${CMAKE_CURRENT_LIST_DIR}/../../idf_cpp_lifecycle")
set(module_build "${CMAKE_CURRENT_BINARY_DIR}/elf_cpp_lifecycle_build")
set(module_elf "${module_build}/elf_cpp_lifecycle.app.elf")
set(module_payload "${CMAKE_CURRENT_BINARY_DIR}/cpp_lifecycle_payload.bin")

ExternalProject_Add(elf_cpp_lifecycle_module
    SOURCE_DIR "${module_path}"
    BINARY_DIR "${module_build}"
    CMAKE_ARGS
        -DIDF_PATH=${idf_path}
        -DIDF_TARGET=${idf_target}
        -DSDKCONFIG=${module_build}/sdkconfig
    CMAKE_GENERATOR "Unix Makefiles"
    BUILD_COMMAND make elf
    BUILD_ALWAYS 1
    BUILD_BYPRODUCTS "${module_elf}"
    INSTALL_COMMAND ""
)
add_custom_command(
    OUTPUT "${module_payload}"
    COMMAND "${CMAKE_COMMAND}" -E copy "${module_elf}" "${module_payload}"
    DEPENDS elf_cpp_lifecycle_module "${module_elf}"
    VERBATIM
)
add_custom_target(elf_cpp_lifecycle_payload ALL DEPENDS "${module_payload}")
add_dependencies(${COMPONENT_LIB} elf_cpp_lifecycle_payload)
elf_embed_aligned_binary(${COMPONENT_LIB} "${module_payload}" BINARY)
