set(req)
if("${IDF_VERSION_MAJOR}.${IDF_VERSION_MINOR}" VERSION_GREATER_EQUAL "5.2.6")
    list(APPEND req "esp_driver_i2c")
else()
    list(APPEND req "driver")
endif()

idf_component_register(SRCS "mcp23017.c"
                       INCLUDE_DIRS "include"
                       REQUIRES ${req})

include(package_manager)
cu_pkg_define_version(${CMAKE_CURRENT_LIST_DIR})
