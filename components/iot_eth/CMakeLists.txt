idf_component_register(SRCS "iot_eth.c" "iot_eth_netif_glue.c"
                    INCLUDE_DIRS "include" "interface"
                    REQUIRES esp_netif)

include(package_manager)
cu_pkg_define_version(${CMAKE_CURRENT_LIST_DIR})
