
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

idf_component_register(SRCS "ble_ota.c"
                        INCLUDE_DIRS include
                        REQUIRES bt)

pkg_check_version(${CMAKE_CURRENT_SOURCE_DIR} ver_major ver_minor ver_patch)
message(STATUS "ble_ota: ${ver_major}.${ver_minor}.${ver_patch}")

target_compile_options(${COMPONENT_LIB} PUBLIC
    -DBLE_OTA_VER_MAJOR=${ver_major} -DBLE_OTA_VER_MINOR=${ver_minor} -DBLE_OTA_VER_PATCH=${ver_patch})
