idf_component_register(SRCS "iot_knob.c" 
                       INCLUDE_DIRS "."
                       REQUIRES driver
                       PRIV_REQUIRES esp_timer)

function(pkg_check_version file_path ver_major ver_minor ver_patch)
    set(file_name "${file_path}/idf_component.yml")
    if(EXISTS ${file_name})
        file(READ ${file_name} ver)
        string(REGEX MATCH "(^|\n)version: \"?([0-9]+).([0-9]+).([0-9]+)\"?" _ ${ver})
        set(${ver_major} ${CMAKE_MATCH_2} PARENT_SCOPE)
        set(${ver_minor} ${CMAKE_MATCH_3} PARENT_SCOPE)
        set(${ver_patch} ${CMAKE_MATCH_4} PARENT_SCOPE)
    else()
        message(WARNING " ${file_name} not exist")
    endif()
endfunction()

pkg_check_version(${CMAKE_CURRENT_SOURCE_DIR} ver_major ver_minor ver_patch)
message(STATUS "knob: ${ver_major}.${ver_minor}.${ver_patch}")

target_compile_options(${COMPONENT_LIB} PUBLIC
    -DKNOB_VER_MAJOR=${ver_major} -DKNOB_VER_MINOR=${ver_minor} -DKNOB_VER_PATCH=${ver_patch})