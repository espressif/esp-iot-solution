idf_component_register(SRCS "max17048.c"
                        INCLUDE_DIRS include)

include(package_manager)
cu_pkg_define_version(${CMAKE_CURRENT_LIST_DIR})
