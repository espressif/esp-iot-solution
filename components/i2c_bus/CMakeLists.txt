if("${IDF_VERSION_MAJOR}.${IDF_VERSION_MINOR}" VERSION_GREATER_EQUAL "5.3")
    set(SRC_FILE "i2c_bus_v2.c")
    set(REQ esp_driver_i2c)
else()
    set(SRC_FILE "i2c_bus.c")
    set(REQ driver)
endif()

idf_component_register(SRCS ${SRC_FILE}
                        INCLUDE_DIRS "include"
                        REQUIRES ${REQ})
include(package_manager)
cu_pkg_define_version(${CMAKE_CURRENT_LIST_DIR})
