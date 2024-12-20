idf_component_register(SRC_DIRS "."
                       INCLUDE_DIRS "include"
                       PRIV_INCLUDE_DIRS "priv_include"
                       REQUIRES esp_timer driver esp_event
                       )

include(package_manager)
cu_pkg_define_version(${CMAKE_CURRENT_LIST_DIR})
