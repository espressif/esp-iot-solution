if(CONFIG_GPROF_ENABLE)
    set(srcs
        "src/esp_gprof.c")

    set(include_dirs "include")
    set(priv_include_dir "private_include")
endif()

idf_component_register(SRCS ${srcs}
                       INCLUDE_DIRS ${include_dirs}
                       PRIV_INCLUDE_DIRS ${priv_include_dir}
                       PRIV_REQUIRES
                            "esp_partition"
                            "lwip"          # for sys/socket.h and iovec
                       )

include(package_manager)
if(CONFIG_GPROF_ENABLE)
    cu_pkg_define_version(${CMAKE_CURRENT_LIST_DIR})
endif()
