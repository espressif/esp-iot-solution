idf_component_register(
    SRC_DIRS ./src
    INCLUDE_DIRS ./include ./src)

include(package_manager)
cu_pkg_define_version(${CMAKE_CURRENT_LIST_DIR})
