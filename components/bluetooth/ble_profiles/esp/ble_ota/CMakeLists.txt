set(srcs "")
set(priv_include_dirs "")
set(requires bt nvs_flash protocomm esp_netif protobuf-c)
set(priv_requires json esp_timer)

if(CONFIG_BT_NIMBLE_ENABLED)
    list(APPEND srcs "src/nimble_ota.c")

    if(CONFIG_OTA_WITH_PROTOCOMM)
        list(APPEND srcs
            "src/scheme_ble.c"
            "src/manager.c"
            "src/handlers.c"
            "src/decrypt_ota_file.c"
            "proto-c/ota_send_file.pb-c.c")
        list(APPEND priv_include_dirs proto-c "${IDF_PATH}/components/protocomm/proto-c")
    endif()
else()
    list(APPEND srcs "src/ble_ota.c")
endif()

idf_component_register(SRCS "${srcs}"
                       INCLUDE_DIRS "include"
                       PRIV_INCLUDE_DIRS "${priv_include_dirs}"
                       REQUIRES "${requires}"
                       PRIV_REQUIRES "${priv_requires}")

include(package_manager)
cu_pkg_define_version(${CMAKE_CURRENT_LIST_DIR})
