idf_component_register(SRC_DIRS "."
                        INCLUDE_DIRS "include")

include(package_manager)
cu_pkg_define_version(${CMAKE_CURRENT_LIST_DIR})
target_compile_options(${COMPONENT_LIB} PRIVATE -Wno-type-limits)
