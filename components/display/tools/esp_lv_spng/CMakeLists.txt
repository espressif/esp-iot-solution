idf_component_register(
    SRCS "esp_lv_spng.c"
    INCLUDE_DIRS "include"
)

include(package_manager)
cu_pkg_define_version(${CMAKE_CURRENT_LIST_DIR})
