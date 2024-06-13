idf_component_register(
    SRCS "esp_mmap_assets.c"
    INCLUDE_DIRS "include"
    REQUIRES esp_partition
    PRIV_REQUIRES spi_flash
)

include(package_manager)
cu_pkg_define_version(${CMAKE_CURRENT_LIST_DIR})
