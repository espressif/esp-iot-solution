cmake_minimum_required(VERSION 3.16)

set(srcs "src/bootloader_custom_image_format.c"
         "src/bootloader_custom_ota.c"
         "src/bootloader_custom_utility.c"
         )

set(requires "bootloader" "bootloader_support" "spi_flash")

# Since IDF v6.0 the watchdog HAL has graduated into its own component
# (esp_hal_wdt). In earlier IDF versions hal/wdt_hal.h is provided by the
# hal component, which is reachable through the existing requirements.
# Flash encryption status also moved to the efuse component in IDF v6.0.
if("${IDF_VERSION_MAJOR}.${IDF_VERSION_MINOR}" VERSION_GREATER_EQUAL "6.0")
    list(APPEND requires "esp_hal_wdt" "efuse")
endif()

if(BOOTLOADER_BUILD)
    list(APPEND srcs "src/bootloader_custom_malloc.c"
                     "src/bootloader_decompressor_common.c"
                     "src/bootloader_storage_flash.c")
else()
    list(APPEND requires "app_update" "esp_partition")
endif()

if(CONFIG_BOOTLOADER_DECOMPRESSOR_XZ)
    list(APPEND srcs "src/bootloader_decompressor_xz.c")
    list(APPEND requires "xz")
endif()

# ESP32-P4 is supported from IDF v5.5.5 and only for chip revision >= 3.
# CONFIG_ESP32P4_SELECTS_REV_LESS_V3 selects the rev < 3 memory map.
if(CONFIG_IDF_TARGET_ESP32P4)
    if(DEFINED IDF_VERSION_PATCH)
        set(_bsp_idf_ver "${IDF_VERSION_MAJOR}.${IDF_VERSION_MINOR}.${IDF_VERSION_PATCH}")
    else()
        set(_bsp_idf_ver "${IDF_VERSION_MAJOR}.${IDF_VERSION_MINOR}.0")
    endif()
    if("${_bsp_idf_ver}" VERSION_LESS "5.5.5")
        message(FATAL_ERROR
            "bootloader_support_plus: ESP32-P4 requires ESP-IDF >= 5.5.5 "
            "(this build is ${_bsp_idf_ver}).")
    endif()
    if(CONFIG_ESP32P4_SELECTS_REV_LESS_V3)
        message(FATAL_ERROR
            "bootloader_support_plus: ESP32-P4 rev < 3 is not supported. "
            "Disable CONFIG_ESP32P4_SELECTS_REV_LESS_V3 and set the chip revision to 3 or newer.")
    endif()
endif()

# ESP32-S31 bootloader scripts start at IDF v6.1 (generated .ld.in, no linker.lf).
if(CONFIG_IDF_TARGET_ESP32S31)
    if("${IDF_VERSION_MAJOR}.${IDF_VERSION_MINOR}" VERSION_LESS "6.1")
        message(FATAL_ERROR
            "bootloader_support_plus: ESP32-S31 requires ESP-IDF >= 6.1 "
            "(this build is ${IDF_VERSION_MAJOR}.${IDF_VERSION_MINOR}).")
    endif()
endif()

set(priv_include "private_include")

idf_component_get_property(bootloader_support_dir bootloader_support COMPONENT_DIR)

if(EXISTS "${bootloader_support_dir}/private_include")
    list(APPEND priv_include "${bootloader_support_dir}/private_include")
else()
    list(APPEND priv_include "${bootloader_support_dir}/include_bootloader")
endif()

idf_component_register(SRCS ${srcs}
                    INCLUDE_DIRS "include"
                    PRIV_INCLUDE_DIRS ${priv_include}
                    PRIV_REQUIRES ${requires})

# Attach the IDF v6.0 sections-script injection once the bootloader ELF target
# exists. CMake v2 creates that target only after components are included.
# Paths are stored in GLOBAL properties because cmake_language(DEFER CALL)
# argument forwarding is unreliable across directory scopes.
function(_bootloader_support_plus_attach_loader_rules)
    get_property(inject_script GLOBAL PROPERTY _BSP_XZ_LOADER_INJECT_SCRIPT)
    get_property(build_dir GLOBAL PROPERTY _BSP_XZ_LOADER_INJECT_BUILD_DIR)
    if(NOT inject_script OR NOT EXISTS "${inject_script}")
        message(FATAL_ERROR
            "bootloader_support_plus: inject script not set: '${inject_script}'")
    endif()
    if(NOT build_dir)
        message(FATAL_ERROR
            "bootloader_support_plus: inject BUILD_DIR not set")
    endif()
    get_property(idf_target GLOBAL PROPERTY _BSP_XZ_LOADER_INJECT_IDF_TARGET)
    get_property(p4_rev3_source GLOBAL PROPERTY _BSP_XZ_P4_REV3_SOURCE)
    if(NOT idf_target OR idf_target STREQUAL "NOTFOUND")
        set(idf_target "")
    endif()
    if(NOT p4_rev3_source OR p4_rev3_source STREQUAL "NOTFOUND")
        set(p4_rev3_source "")
    endif()

    if(TARGET bootloader_elf)
        set(bootloader_elf bootloader_elf)
    elseif(TARGET bootloader.elf)
        set(bootloader_elf bootloader.elf)
    else()
        message(FATAL_ERROR
            "bootloader_support_plus: bootloader ELF target not found; "
            "cannot inject XZ loader input rules")
    endif()

    add_custom_command(TARGET ${bootloader_elf} PRE_LINK
        COMMAND ${CMAKE_COMMAND}
            "-DBUILD_DIR=${build_dir}"
            "-DIDF_TARGET=${idf_target}"
            "-DP4_REV3_SOURCE=${p4_rev3_source}"
            -P "${inject_script}"
        VERBATIM
        COMMENT "Inject plus/XZ input rules into the bootloader linker script")

    # IDF 5.5.x points -T at the static SDK bootloader.rev3.ld. Retarget that
    # option onto the build-dir copy which the PRE_LINK step above patches.
    if(p4_rev3_source)
        _bootloader_support_plus_use_p4_rev3_copy("${build_dir}/ld/bootloader.rev3.ld")
    endif()
endfunction()

# Drop the rev < 3 bootloader.ld and point bootloader.rev3.ld at the copy.
function(_bootloader_support_plus_use_p4_rev3_copy rev3_copy)
    if(NOT TARGET __idf_main)
        message(FATAL_ERROR
            "bootloader_support_plus: __idf_main not found; "
            "cannot retarget the ESP32-P4 rev>=3 linker script")
    endif()
    get_target_property(opts __idf_main INTERFACE_LINK_OPTIONS)
    if(NOT opts OR opts STREQUAL "opts-NOTFOUND")
        set(opts "")
    endif()

    set(new_opts "")
    set(replaced FALSE)
    foreach(opt IN LISTS opts)
        if(opt MATCHES "SHELL:-T[ \t]+(.*/)?bootloader\\.ld$")
            # rev < 3 map. ESP32-P4 support is rev >= 3 only.
            continue()
        elseif(opt MATCHES "SHELL:-T[ \t]+(.*/)?bootloader\\.rev3\\.ld$")
            if(NOT replaced)
                list(APPEND new_opts "SHELL:-T ${rev3_copy}")
                set(replaced TRUE)
            endif()
        else()
            list(APPEND new_opts "${opt}")
        endif()
    endforeach()
    if(NOT replaced)
        list(APPEND new_opts "SHELL:-T ${rev3_copy}")
    endif()
    set_property(TARGET __idf_main PROPERTY INTERFACE_LINK_OPTIONS ${new_opts})
    message(STATUS "bootloader_support_plus: ESP32-P4 links ${rev3_copy}")
endfunction()

if(BOOTLOADER_BUILD)
    # ---------------------------------------------------------------
    # Custom bootloader linker placement, selected by IDF version.
    # ---------------------------------------------------------------
    # v5.x, except ESP32-P4:
    #   The bootloader linker script is a static .ld file. A .lf file passed
    #   to target_linker_script is accepted by ld as a linker script. A custom
    #   iram_loader1_seg and .xz section hold the XZ decompressor.
    #
    # ESP32-P4, IDF v5.5.5 through v5.x:
    #   linker.lf would overlap iram_loader_seg. Copy the static
    #   bootloader.rev3.ld into the build directory and inject plus/XZ rules
    #   into .iram_loader.text. rev < 3 (bootloader.ld) is rejected above.
    #
    # v6.0 and newer, every target:
    #   The bootloader linker script is generated from .ld.in templates, and
    #   ldgen fragments are not applied. CMake v2 emits the default sections
    #   script before dependency scripts, so an INSERT fragment cannot move
    #   input sections that the default script already matched.
    #   Before linking, archive rules for plus/XZ are injected into
    #   .iram_loader.text (before _loader_text_end, and therefore before the
    #   later .iram.text wildcard).
    #   IDF 6.0/6.0.1 generates bootloader.ld, or bootloader.rev3.ld on
    #   ESP32-P4 rev >= 3. IDF 6.0.2 and newer generate bootloader.sections.ld
    #   (the rev >= 3 addresses live in bootloader.memory.ld). The same
    #   PRE_LINK step covers CMake v1 and v2.
    # ---------------------------------------------------------------
    set(_bsp_use_loader_inject FALSE)
    set(_bsp_p4_rev3_source "")
    if("${IDF_VERSION_MAJOR}.${IDF_VERSION_MINOR}" VERSION_GREATER_EQUAL "6.0")
        set(_bsp_use_loader_inject TRUE)
    elseif(CONFIG_IDF_TARGET_ESP32P4)
        set(_bsp_use_loader_inject TRUE)
        set(_bsp_p4_rev3_source
            "${IDF_PATH}/components/bootloader/subproject/main/ld/esp32p4/bootloader.rev3.ld")
        if(NOT EXISTS "${_bsp_p4_rev3_source}")
            message(FATAL_ERROR
                "bootloader_support_plus: ESP32-P4 rev>=3 linker script not found: "
                "${_bsp_p4_rev3_source}")
        endif()
    endif()

    if(_bsp_use_loader_inject)
        get_property(_bsp_inject_scheduled GLOBAL PROPERTY _BSP_XZ_LOADER_INJECT_SCHEDULED)
        if(NOT _bsp_inject_scheduled)
            set(_bsp_inject_script "${CMAKE_CURRENT_LIST_DIR}/ld/inject_xz_loader_rules.cmake")
            if(NOT EXISTS "${_bsp_inject_script}")
                message(FATAL_ERROR
                    "bootloader_support_plus: missing ${_bsp_inject_script}")
            endif()
            set_property(GLOBAL PROPERTY _BSP_XZ_LOADER_INJECT_SCHEDULED YES)
            set_property(GLOBAL PROPERTY _BSP_XZ_LOADER_INJECT_SCRIPT "${_bsp_inject_script}")
            set_property(GLOBAL PROPERTY _BSP_XZ_LOADER_INJECT_BUILD_DIR "${CMAKE_BINARY_DIR}")
            set_property(GLOBAL PROPERTY _BSP_XZ_LOADER_INJECT_IDF_TARGET "${CONFIG_IDF_TARGET}")
            set_property(GLOBAL PROPERTY _BSP_XZ_P4_REV3_SOURCE "${_bsp_p4_rev3_source}")
            cmake_language(DEFER DIRECTORY "${CMAKE_SOURCE_DIR}"
                CALL _bootloader_support_plus_attach_loader_rules)
        endif()
    else()
        set(bootloader_custom_ld "ld/${CONFIG_IDF_TARGET}/linker.lf")
        if(NOT EXISTS "${CMAKE_CURRENT_LIST_DIR}/${bootloader_custom_ld}")
            message(FATAL_ERROR
                "bootloader_support_plus: custom linker script not found for "
                "target '${CONFIG_IDF_TARGET}': ${bootloader_custom_ld}. "
                "This chip is not (yet) supported by bootloader_support_plus.")
        endif()
        target_linker_script(${COMPONENT_LIB} INTERFACE "${bootloader_custom_ld}")
    endif()
else()
    if(CONFIG_BOOTLOADER_COMPRESSED_ENABLED)
        target_link_libraries(${COMPONENT_LIB} INTERFACE "-Wl,--wrap=esp_ota_get_next_update_partition")
    endif()

    # Force app_update to also appear later than bootloader_support_plus in link line
    idf_component_get_property(app_update app_update COMPONENT_LIB)
    target_link_libraries(${COMPONENT_LIB} INTERFACE $<TARGET_FILE:${app_update}> $<TARGET_FILE:${COMPONENT_LIB}>)

endif()

if(CONFIG_BOOTLOADER_COMPRESSED_ENABLED)
    target_link_libraries(${COMPONENT_LIB} INTERFACE "-Wl,--wrap=esp_image_verify")
endif()

include(package_manager)
cu_pkg_define_version(${CMAKE_CURRENT_LIST_DIR})
