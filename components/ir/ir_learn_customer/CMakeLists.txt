idf_component_register(
    SRCS "ir_encoder.c" "ir_learn.c"
    INCLUDE_DIRS "include"
    PRIV_INCLUDE_DIRS "priv_include"
    REQUIRES driver ${REQ}
    PRIV_REQUIRES esp_timer)

include(package_manager)
cu_pkg_define_version(${CMAKE_CURRENT_LIST_DIR})

target_compile_options(${COMPONENT_LIB} PRIVATE "-Wno-format")
