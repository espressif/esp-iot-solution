set(PRIV_REQUIRES_LIST esp_mm)
if(IDF_TARGET STREQUAL "esp32p4" OR IDF_TARGET STREQUAL "esp32s31")
    list(APPEND PRIV_REQUIRES_LIST esp_driver_jpeg)
endif()

idf_component_register(
    INCLUDE_DIRS "include"
    PRIV_REQUIRES ${PRIV_REQUIRES_LIST}
)

idf_build_get_property(build_components BUILD_COMPONENTS)
if(lvgl IN_LIST build_components)
    set(lvgl_name lvgl)
else()
    set(lvgl_name lvgl__lvgl)
endif()

idf_component_get_property(lvgl_dir ${lvgl_name} COMPONENT_DIR)
set(lvgl_major "")
foreach(header IN ITEMS include/lvgl/lv_version.h lv_version.h lvgl.h)
    if(EXISTS "${lvgl_dir}/${header}")
        file(STRINGS "${lvgl_dir}/${header}" version_define
             REGEX "^[ \t]*#[ \t]*define[ \t]+LVGL_VERSION_MAJOR[ \t]+[0-9]+")
        if(version_define)
            string(REGEX MATCH "[0-9]+" lvgl_major "${version_define}")
            break()
        endif()
    endif()
endforeach()
if(NOT lvgl_major)
    message(FATAL_ERROR "Cannot determine the LVGL major version from ${lvgl_dir}")
endif()

set(ADD_LIBS idf::esp_mm)
if(lvgl_major EQUAL 8)
    set(DECODER_FOLDER "lvgl8")
elseif(lvgl_major EQUAL 9)
    set(DECODER_FOLDER "lvgl9")
else()
    message(FATAL_ERROR "Unsupported LVGL major version: ${lvgl_major}")
endif()
message(STATUS "ESP LV Decoder: LVGL ${lvgl_major}")

set(DECODER_PARH "src/${DECODER_FOLDER}")

if("espressif__esp_new_jpeg" IN_LIST build_components)
    list(APPEND ADD_LIBS idf::espressif__esp_new_jpeg)
elseif("esp_new_jpeg" IN_LIST build_components)
    list(APPEND ADD_LIBS idf::esp_new_jpeg)
else()
    message(FATAL_ERROR "esp_lv_decoder requires esp_new_jpeg. Enable Component Manager or add esp_new_jpeg locally.")
endif()

if("espressif__libpng" IN_LIST build_components)
    list(APPEND ADD_LIBS idf::espressif__libpng)
endif()
if("libpng" IN_LIST build_components)
    list(APPEND ADD_LIBS idf::libpng)
endif()

if("espressif__qoi" IN_LIST build_components)
    list(APPEND ADD_LIBS idf::espressif__qoi)
endif()
if("qoi" IN_LIST build_components)
    list(APPEND ADD_LIBS idf::qoi)
endif()

if(IDF_TARGET STREQUAL "esp32p4" OR IDF_TARGET STREQUAL "esp32s31")
    list(APPEND ADD_LIBS idf::esp_driver_jpeg)
endif()

# Here we create the real lvgl_docoder_lib
add_library(lvgl_docoder_lib STATIC
    ${DECODER_PARH}/esp_lv_decoder.c
    src/esp_lv_decoder_pjpg.c
    ${ADD_SRCS}
    )
target_include_directories(lvgl_docoder_lib PUBLIC "include")
target_include_directories(lvgl_docoder_lib PRIVATE "src")
target_link_libraries(lvgl_docoder_lib PUBLIC
    idf::${lvgl_name}
    )
target_link_libraries(lvgl_docoder_lib PRIVATE
    ${ADD_LIBS}
    )

# Finally, link the lvgl_docoder_lib its esp-idf interface library
target_link_libraries(${COMPONENT_LIB} INTERFACE lvgl_docoder_lib)
