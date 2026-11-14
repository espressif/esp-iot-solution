# SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
# SPDX-License-Identifier: Apache-2.0

cmake_minimum_required(VERSION 3.16)
project(elf_registry_retention C)

# Verify that LTO and dead-section elimination cannot drop an unreferenced
# registry when the real component's INTERFACE link option is applied.
set(CMAKE_INTERPROCEDURAL_OPTIMIZATION TRUE)
add_compile_options(-ffunction-sections -fdata-sections)
add_link_options(-Wl,--gc-sections)

get_filename_component(ELF_LOADER_DIR "${CMAKE_CURRENT_LIST_DIR}/../../../.." ABSOLUTE)
set(CONFIG_ELF_LOADER ON)
set(COMPONENT_LIB elf_loader_host)

# Stand-ins for ESP-IDF registration and package-manager hooks; the actual
# component CMakeLists.txt below supplies its production link option.
file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/package_manager.cmake" "# host-only stub\n")
list(PREPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_BINARY_DIR}")

function(idf_component_register)
    add_library(${COMPONENT_LIB} STATIC "${ELF_LOADER_DIR}/src/esp_elf_symbol_registry.c")
    target_include_directories(${COMPONENT_LIB} PUBLIC
        "${ELF_LOADER_DIR}/include" "${ELF_LOADER_DIR}/tests/host/include")
    target_compile_definitions(${COMPONENT_LIB} PRIVATE
        CONFIG_IDF_TARGET_ARCH_RISCV=1 CONFIG_ELF_LOADER_LIBC_SYMBOLS=1)
endfunction()

function(cu_pkg_define_version)
endfunction()

include("${ELF_LOADER_DIR}/CMakeLists.txt")

# Negative control: compile identical source into a normal static archive,
# without propagating the component's forced-undefined linker option.
add_library(registry_without_force STATIC "${ELF_LOADER_DIR}/src/esp_elf_symbol_registry.c")
target_include_directories(registry_without_force PUBLIC
    "${ELF_LOADER_DIR}/include" "${ELF_LOADER_DIR}/tests/host/include")
target_compile_definitions(registry_without_force PRIVATE
    CONFIG_IDF_TARGET_ARCH_RISCV=1 CONFIG_ELF_LOADER_LIBC_SYMBOLS=1)

add_executable(with_force test_registry_retention_main.c)
target_link_libraries(with_force PRIVATE ${COMPONENT_LIB})

add_executable(without_force test_registry_retention_main.c)
target_link_libraries(without_force PRIVATE registry_without_force)
