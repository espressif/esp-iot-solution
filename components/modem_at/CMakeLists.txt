idf_component_register(SRC_DIRS "src" "src/modules"
                    INCLUDE_DIRS "include" "src/modules"
                    )

include(package_manager)
cu_pkg_define_version(${CMAKE_CURRENT_LIST_DIR})
