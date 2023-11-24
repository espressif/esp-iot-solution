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

idf_component_register(SRCS ${srcs}
                       INCLUDE_DIRS ${include_dir}
                       PRIV_INCLUDE_DIRS ${priv_include_dir}
                       PRIV_REQUIRES vfs driver)

include(package_manager)
cu_pkg_define_version(${CMAKE_CURRENT_LIST_DIR})
