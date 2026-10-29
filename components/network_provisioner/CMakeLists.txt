idf_build_get_property(target IDF_TARGET)

if(${target} STREQUAL "linux")
    return()
endif()

set(srcs
    "src/network_provisioner.c"
    "src/network_provisioner_ble.c"
    "src/network_config_client.c"
    "src/network_scan_client.c"
    "src/network_ctrl_client.c"
    "proto-c/network_config.pb-c.c"
    "proto-c/network_scan.pb-c.c"
    "proto-c/network_ctrl.pb-c.c"
    "proto-c/network_constants.pb-c.c")

idf_component_register(SRCS "${srcs}"
                    INCLUDE_DIRS "include" "proto-c"
                    PRIV_INCLUDE_DIRS "private_include"
                    REQUIRES protocomm_ext
                    PRIV_REQUIRES protobuf-c)

if(CONFIG_BT_NIMBLE_ENABLED)
    idf_component_optional_requires(PRIVATE bt)
endif()
