idf_build_get_property(target IDF_TARGET)

if(${target} STREQUAL "linux")
    return() # This component is not supported by the POSIX/Linux simulator
endif()

set(include_dirs include/common
                 include/security
                 include/transports
                 proto-c)

set(priv_include_dirs private_include/common
                      private_include/security)

set(srcs
    "src/common/protocomm_ext.c"
    "proto-c/constants.pb-c.c"
    "proto-c/sec0.pb-c.c"
    "proto-c/sec1.pb-c.c"
    "proto-c/sec2.pb-c.c"
    "proto-c/session.pb-c.c"
    "src/transports/http/protocomm_ext_http.c"
    "src/transports/console/protocomm_ext_console.c")

if(CONFIG_ESP_PROTOCOMM_EXT_SUPPORT_SECURITY_VERSION_0)
    list(APPEND srcs
        "src/security/protocomm_ext_security0.c")
endif()

if(CONFIG_ESP_PROTOCOMM_EXT_SUPPORT_SECURITY_VERSION_1)
    list(APPEND srcs
        "src/security/protocomm_ext_security1.c")
endif()

if(CONFIG_ESP_PROTOCOMM_EXT_SUPPORT_SECURITY_VERSION_2)
    list(APPEND srcs
        "src/security/protocomm_ext_security2.c"
        "src/security/srp6a/protocomm_ext_srp6a_client.c")
endif()

# Always REQUIRES bt so NimBLE public headers resolve when the app enables BT.
# (CONFIG_BT_* may be unavailable during early component CMake evaluation.)
# NimBLE transport sources remain gated on CONFIG_BT_NIMBLE_ENABLED.
if(CONFIG_BT_ENABLED AND CONFIG_BT_NIMBLE_ENABLED)
    list(APPEND srcs
        "src/transports/nimble/protocomm_ext_nimble.c"
        "src/transports/nimble/nimble_central_utils/protocomm_ext_nimble_peer.c")
    list(APPEND priv_include_dirs "src/transports/nimble/nimble_central_utils")
endif()

idf_component_register(SRCS "${srcs}"
                    INCLUDE_DIRS "${include_dirs}"
                    PRIV_INCLUDE_DIRS "${priv_include_dirs}"
                    PRIV_REQUIRES protobuf-c mbedtls esp_http_client esp_driver_uart
                    REQUIRES bt)
