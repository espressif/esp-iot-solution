idf_component_register(SRC_DIRS "src" "."
                        INCLUDE_DIRS "include"
                        REQUIRES esp_driver_gpio esp_driver_gptimer)

include(package_manager)
cu_pkg_define_version(${CMAKE_CURRENT_LIST_DIR})
