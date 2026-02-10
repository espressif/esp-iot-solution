idf_component_register(
    SRCS "esp_lv_fs.c"
    INCLUDE_DIRS "include"
    REQUIRES esp_mmap_assets
)

include(package_manager)
cu_pkg_define_version(${CMAKE_CURRENT_LIST_DIR})
