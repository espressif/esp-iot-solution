idf_component_register(
    SRC_DIRS "."
    INCLUDE_DIRS "."
)

include(package_manager)
cu_pkg_define_version(${CMAKE_CURRENT_LIST_DIR})
