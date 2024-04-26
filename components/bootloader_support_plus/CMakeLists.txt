set(srcs "src/bootloader_custom_image_format.c"
         "src/bootloader_custom_ota.c"
         "src/bootloader_custom_utility.c"
         )

set(requires "bootloader" "bootloader_support" "spi_flash")

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

if(BOOTLOADER_BUILD)
    if (CONFIG_IDF_TARGET_ESP32C2 OR CONFIG_IDF_TARGET_ESP32C3 OR CONFIG_IDF_TARGET_ESP32C6 OR CONFIG_IDF_TARGET_ESP32H2 OR CONFIG_IDF_TARGET_ESP32)
        target_linker_script(${COMPONENT_LIB} INTERFACE "ld/${CONFIG_IDF_TARGET}/linker.lf")
    endif()
else()
    if(CONFIG_BOOTLOADER_COMPRESSED_ENABLED)
        target_link_libraries(${COMPONENT_LIB} INTERFACE "-Wl,--wrap=esp_ota_get_next_update_partition")
    endif()

    # Force app_update to also appear later than bootloader_support_plus in link line
    idf_component_get_property(app_update app_update COMPONENT_LIB)
    idf_build_get_property(build_components BUILD_COMPONENTS)
    if("espressif__bootloader_support_plus" IN_LIST build_components)
        idf_component_get_property(bootloader_support_plus espressif__bootloader_support_plus COMPONENT_LIB)
    else()
        idf_component_get_property(bootloader_support_plus bootloader_support_plus COMPONENT_LIB)
    endif()
    target_link_libraries(${COMPONENT_LIB} INTERFACE $<TARGET_FILE:${app_update}> $<TARGET_FILE:${bootloader_support_plus}>)

endif()

if(CONFIG_BOOTLOADER_COMPRESSED_ENABLED)
    target_link_libraries(${COMPONENT_LIB} INTERFACE "-Wl,--wrap=esp_image_verify")
endif()

include(package_manager)
cu_pkg_define_version(${CMAKE_CURRENT_LIST_DIR})
