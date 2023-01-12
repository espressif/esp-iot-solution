function(pkg_check_version file_path ver_major ver_minor ver_patch)
set(file_name "${file_path}/idf_component.yml")
if(EXISTS ${file_name})
    file(READ ${file_name} ver)
    string(REGEX MATCH "version: \"([0-9]+).([0-9]+).([0-9]+)\"" _ ${ver})
    set(${ver_major} ${CMAKE_MATCH_1} PARENT_SCOPE)
    set(${ver_minor} ${CMAKE_MATCH_2} PARENT_SCOPE)
    set(${ver_patch} ${CMAKE_MATCH_3} PARENT_SCOPE)
else()
    message(WARNING " ${file_name} not exist")
endif()
endfunction()

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

pkg_check_version(${CMAKE_CURRENT_SOURCE_DIR} ver_major ver_minor ver_patch)
message(STATUS "ble_ota: ${ver_major}.${ver_minor}.${ver_patch}")

target_compile_options(${COMPONENT_LIB} PUBLIC
    -DBLE_OTA_VER_MAJOR=${ver_major} -DBLE_OTA_VER_MINOR=${ver_minor} -DBLE_OTA_VER_PATCH=${ver_patch})
