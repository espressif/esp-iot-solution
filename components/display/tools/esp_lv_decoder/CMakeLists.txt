idf_component_register(
    SRCS "esp_lv_decoder.c"
    INCLUDE_DIRS "include"
    PRIV_INCLUDE_DIRS "priv_include"
)

include(package_manager)
cu_pkg_define_version(${CMAKE_CURRENT_LIST_DIR})
