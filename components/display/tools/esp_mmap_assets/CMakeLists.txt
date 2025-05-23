set(PRIV_REQUIRES_LIST spi_flash esp_partition app_update)

if("${IDF_VERSION_MAJOR}.${IDF_VERSION_MINOR}" VERSION_LESS "5.5")
    list(APPEND bootloader_support)
else()
    list(APPEND esp_bootloader_format)
endif()

idf_component_register(
    SRCS "esp_mmap_assets.c"
    INCLUDE_DIRS "include"
    PRIV_REQUIRES ${PRIV_REQUIRES_LIST}
)

include(package_manager)
cu_pkg_define_version(${CMAKE_CURRENT_LIST_DIR})
