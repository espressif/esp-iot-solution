if("${IDF_VERSION_MAJOR}.${IDF_VERSION_MINOR}" VERSION_LESS "5.3" OR CONFIG_I2C_BUS_BACKWARD_CONFIG)
    set(SRC_FILE "i2c_bus.c")
    set(REQ driver)
else()
    set(SRC_FILE "i2c_bus_v2.c")
    set(REQ esp_driver_i2c driver)
endif()

if (CONFIG_I2C_BUS_SUPPORT_SOFTWARE)
    list(APPEND SRC_FILE "i2c_bus_soft.c")
endif()

idf_component_register(SRCS ${SRC_FILE}
                        INCLUDE_DIRS "include"
                        PRIV_INCLUDE_DIRS "private_include"
                        REQUIRES ${REQ})

include(package_manager)
cu_pkg_define_version(${CMAKE_CURRENT_LIST_DIR})
