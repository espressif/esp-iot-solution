idf_component_register(
    INCLUDE_DIRS "include"
    PRIV_INCLUDE_DIRS "priv_include"
)

# Get LVGL version
idf_build_get_property(build_components BUILD_COMPONENTS)
if(lvgl IN_LIST build_components)
    set(lvgl_name lvgl) # Local component
    set(lvgl_ver $ENV{LVGL_VERSION}) # Get the version from env variable (set from LVGL v9.2)
else()
    set(lvgl_name lvgl__lvgl) # Managed component
    idf_component_get_property(lvgl_ver ${lvgl_name} COMPONENT_VERSION) # Get the version from esp-idf build system
endif()

if("${lvgl_ver}" STREQUAL "")
    message("Could not determine LVGL version, assuming v9.x")
endif()

# Select folder by LVGL version
set(ADD_LIBS "")
message(STATUS "LVGL version: ${lvgl_ver}")
if(lvgl_ver VERSION_LESS "9.0.0")
    message(VERBOSE "Compiling decoder for LVGL8")
    set(DECODER_FOLDER "lvgl8")
    list(APPEND ADD_LIBS idf::esp_mm)
    else()
    message(VERBOSE "Compiling decoder for LVGL9")
    set(DECODER_FOLDER "lvgl9")
    list(APPEND ADD_LIBS idf::esp_mm)
endif()

set(DECODER_PARH "src/${DECODER_FOLDER}")

if("espressif__esp_new_jpeg" IN_LIST build_components)
    list(APPEND ADD_LIBS idf::espressif__esp_new_jpeg)
endif()
if("esp_new_jpeg" IN_LIST build_components)
    list(APPEND ADD_LIBS idf::esp_new_jpeg)
endif()

if("espressif__libpng" IN_LIST build_components)
    list(APPEND ADD_LIBS idf::espressif__libpng)
endif()
if("libpng" IN_LIST build_components)
    list(APPEND ADD_LIBS idf::libpng)
endif()

if(${target} STREQUAL "esp32p4")
    list(APPEND ADD_LIBS idf::esp_driver_jpeg)
endif()

# Here we create the real lvgl_docoder_lib
add_library(lvgl_docoder_lib STATIC
    ${DECODER_PARH}/esp_lv_decoder.c
    ${ADD_SRCS}
    )
target_include_directories(lvgl_docoder_lib PUBLIC "include")
target_include_directories(lvgl_docoder_lib PRIVATE "priv_include")
target_link_libraries(lvgl_docoder_lib PUBLIC
    idf::${lvgl_name}
    )
target_link_libraries(lvgl_docoder_lib PRIVATE
    ${ADD_LIBS}
    )

# Finally, link the lvgl_docoder_lib its esp-idf interface library
target_link_libraries(${COMPONENT_LIB} INTERFACE lvgl_docoder_lib)
