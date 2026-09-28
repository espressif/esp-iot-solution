if(CONFIG_EXTENDED_VFS)
    set(srcs  "src/ext_vfs.c")
    set(include_dir "include")
    set(priv_include_dir "private_include")

    if(CONFIG_EXTENDED_VFS_GPIO)
        list(APPEND srcs "src/ext_vfs_gpio.c")
    endif()

    if(CONFIG_EXTENDED_VFS_I2C)
        list(APPEND srcs "src/ext_vfs_i2c.c")
    endif()

    if(CONFIG_EXTENDED_VFS_LEDC)
        list(APPEND srcs "src/ext_vfs_ledc.c")
    endif()

    if(CONFIG_EXTENDED_VFS_SPI)
        list(APPEND srcs "src/ext_vfs_spi.c")
    endif()
endif()

# Driver PRIV_REQUIRES must not use CONFIG_EXTENDED_VFS_*: early requirement expansion runs
# before sdkconfig is applied, so those CONFIGs are false there and deps would be missing
# on the real compile (e.g. driver/gpio.h) even when Kconfig enables all peripherals.
if(NOT DEFINED IDF_VERSION_MAJOR OR "${IDF_VERSION_MAJOR}" STREQUAL "")
    if(DEFINED ENV{IDF_PATH} AND EXISTS "$ENV{IDF_PATH}/tools/cmake/version.cmake")
        include("$ENV{IDF_PATH}/tools/cmake/version.cmake")
    elseif(DEFINED IDF_PATH AND EXISTS "${IDF_PATH}/tools/cmake/version.cmake")
        include("${IDF_PATH}/tools/cmake/version.cmake")
    endif()
endif()

set(priv_reqs "")

set(_extended_vfs_idf_ver_ok FALSE)
if(DEFINED IDF_VERSION_MAJOR AND NOT "${IDF_VERSION_MAJOR}" STREQUAL "" AND
   DEFINED IDF_VERSION_MINOR AND NOT "${IDF_VERSION_MINOR}" STREQUAL "")
    set(_extended_vfs_idf_ver_ok TRUE)
endif()

if(_extended_vfs_idf_ver_ok AND
   "${IDF_VERSION_MAJOR}.${IDF_VERSION_MINOR}" VERSION_GREATER_EQUAL "5.3")
    list(APPEND priv_reqs
         esp_driver_gpio
         esp_driver_i2c
         esp_driver_ledc
         esp_driver_spi)
else()
    # ESP-IDF < 5.3, or version vars unavailable — legacy driver
    list(APPEND priv_reqs driver)
endif()

idf_component_register(SRCS ${srcs}
                       INCLUDE_DIRS ${include_dir}
                       PRIV_INCLUDE_DIRS ${priv_include_dir}
                       REQUIRES vfs
                       PRIV_REQUIRES ${priv_reqs})

include(package_manager)
cu_pkg_define_version(${CMAKE_CURRENT_LIST_DIR})
