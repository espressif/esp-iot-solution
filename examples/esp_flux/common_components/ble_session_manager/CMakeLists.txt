set(SRC_FILES "ble_manager.c" "gatt_discovery.c")

idf_component_register(SRCS ${SRC_FILES}
                        INCLUDE_DIRS include
                        REQUIRES nvs_flash freertos bt)

include(package_manager)
cu_pkg_define_version(${CMAKE_CURRENT_LIST_DIR})
