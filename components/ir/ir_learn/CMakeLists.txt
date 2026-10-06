if("${IDF_VERSION_MAJOR}.${IDF_VERSION_MINOR}" VERSION_GREATER_EQUAL "5.5")
    set(REQ esp_driver_rmt)
else()
    set(REQ driver)
endif()

idf_component_register(
    SRCS "ir_encoder.c" "ir_learn.c"
    INCLUDE_DIRS "include"
    PRIV_INCLUDE_DIRS "priv_include"
    REQUIRES ${REQ}
    PRIV_REQUIRES esp_timer)

include(package_manager)
cu_pkg_define_version(${CMAKE_CURRENT_LIST_DIR})

target_compile_options(${COMPONENT_LIB} PRIVATE "-Wno-format")
