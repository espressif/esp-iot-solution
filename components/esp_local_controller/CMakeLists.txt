idf_build_get_property(target IDF_TARGET)

if(${target} STREQUAL "linux")
    return()
endif()

# Status enum / status__descriptor come from protocomm_ext (constants.pb-c.*).
# Do not compile a second copy of constants.pb-c.c here (duplicate symbols).
set(srcs
    "src/esp_local_controller.c"
    "proto-c/esp_local_ctrl.pb-c.c")

idf_component_register(SRCS "${srcs}"
                    INCLUDE_DIRS "include" "proto-c"
                    PRIV_INCLUDE_DIRS "private_include"
                    REQUIRES protocomm_ext
                    PRIV_REQUIRES protobuf-c)
