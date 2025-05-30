
if(CONFIG_ELF_LOADER)
    set(srcs  "src/esp_elf_symbol.c"
              "src/esp_elf.c"
              "src/esp_elf_adapter.c")

    if(CONFIG_ELF_LOADER_CUSTOMER_SYMBOLS)
        list(APPEND srcs "src/esp_all_symbol.c")
    endif()

    if(CONFIG_IDF_TARGET_ARCH_XTENSA)
        list(APPEND srcs "src/arch/esp_elf_xtensa.c")

        # ESP32-S2 need to set MMU to run ELF
        if(CONFIG_IDF_TARGET_ESP32S2 AND (CONFIG_ELF_LOADER_LOAD_PSRAM))
            list(APPEND srcs "src/soc/esp_elf_esp32s2.c")
        endif()
    elseif(CONFIG_IDF_TARGET_ARCH_RISCV)
        list(APPEND srcs "src/arch/esp_elf_riscv.c")
    endif()

    set(include_dirs  "include")
    set(ldfragments "linker.lf")
endif()

if(CONFIG_IDF_TARGET_ESP32P4)
    set(priv_req spi_flash esp_mm)
else()
    set(priv_req spi_flash)
endif()

idf_component_register(SRCS ${srcs}
                       INCLUDE_DIRS ${include_dirs}
                       PRIV_REQUIRES spi_flash ${priv_req}
                       LDFRAGMENTS ${ldfragments})

include(package_manager)
if(CONFIG_ELF_LOADER)
    cu_pkg_define_version(${CMAKE_CURRENT_LIST_DIR})
endif()
