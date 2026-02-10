
idf_component_register(SRC_DIRS "src"
                    INCLUDE_DIRS "include"
                    PRIV_INCLUDE_DIRS "private_include"
                    REQUIRES driver
                    PRIV_REQUIRES esp_event)

include(package_manager)
cu_pkg_define_version(${CMAKE_CURRENT_LIST_DIR})
