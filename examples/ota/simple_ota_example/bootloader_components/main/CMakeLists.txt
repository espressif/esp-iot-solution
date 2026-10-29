idf_component_register(SRCS "bootloader_start.c"
                    REQUIRES bootloader bootloader_support)

# ESP32-P4 compressed OTA needs the rev >= 3 bootloader map, which starts
# shipping in ESP-IDF v5.5.5. rev < 3 uses bootloader.ld and is not supported.
if(CONFIG_IDF_TARGET_ESP32P4)
    if(DEFINED IDF_VERSION_PATCH)
        set(_ota_idf_ver "${IDF_VERSION_MAJOR}.${IDF_VERSION_MINOR}.${IDF_VERSION_PATCH}")
    else()
        set(_ota_idf_ver "${IDF_VERSION_MAJOR}.${IDF_VERSION_MINOR}.0")
    endif()
    if("${_ota_idf_ver}" VERSION_LESS "5.5.5")
        message(FATAL_ERROR
            "simple_ota bootloader: ESP32-P4 requires ESP-IDF >= 5.5.5 "
            "(this build is ${_ota_idf_ver}).")
    endif()
    if(CONFIG_ESP32P4_SELECTS_REV_LESS_V3)
        message(FATAL_ERROR
            "simple_ota bootloader: ESP32-P4 rev < 3 is not supported. "
            "Disable CONFIG_ESP32P4_SELECTS_REV_LESS_V3 and set the chip revision to 3 or newer.")
    endif()
endif()

if(CONFIG_IDF_TARGET_ESP32S31)
    if("${IDF_VERSION_MAJOR}.${IDF_VERSION_MINOR}" VERSION_LESS "6.1")
        message(FATAL_ERROR
            "simple_ota bootloader: ESP32-S31 requires ESP-IDF >= 6.1 "
            "(this build is ${IDF_VERSION_MAJOR}.${IDF_VERSION_MINOR}).")
    endif()
endif()

idf_build_get_property(target IDF_TARGET)
set(_ota_ld_dir "${IDF_PATH}/components/bootloader/subproject/main/ld/${target}")

if("${IDF_VERSION_MAJOR}.${IDF_VERSION_MINOR}" VERSION_GREATER_EQUAL "6.0")
    # IDF 6.0.2+ publishes memory + sections scripts in BOOTLOADER_LINKER_SCRIPT
    # (CMake v2). IDF 6.0/6.0.1 leaves the property empty and the bootloader
    # subproject attaches bootloader.rev3.ld.in (ESP32-P4 rev >= 3) or
    # bootloader.ld.in after project(). Do not add bootloader.ld.in here:
    # on ESP32-P4 that is the rev < 3 map.
    idf_build_get_property(scripts BOOTLOADER_LINKER_SCRIPT)
    if(scripts)
        target_linker_script(${COMPONENT_LIB} INTERFACE "${scripts}")
    endif()
else()
    # Static scripts. ESP32-P4 rev >= 3 is bootloader.rev3.ld; other chips
    # stay on bootloader.ld. bootloader_support_plus copies rev3 into the
    # build directory and retargets -T at that copy before linking.
    if(CONFIG_IDF_TARGET_ESP32P4)
        set(scripts "${_ota_ld_dir}/bootloader.rev3.ld"
                    "${_ota_ld_dir}/bootloader.rom.ld")
    else()
        set(scripts "${_ota_ld_dir}/bootloader.ld"
                    "${_ota_ld_dir}/bootloader.rom.ld")
    endif()
    target_linker_script(${COMPONENT_LIB} INTERFACE "${scripts}")
endif()
