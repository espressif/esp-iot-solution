include($ENV{IDF_PATH}/tools/cmake/version.cmake)

set(ADAPTER_PRIV_REQ esp_timer esp_mm)
idf_build_get_property(target IDF_TARGET)
if(${target} STREQUAL "esp32p4")
    list(APPEND ADAPTER_PRIV_REQ esp_driver_ppa)
endif()

idf_build_get_property(build_components BUILD_COMPONENTS)

set(lvgl_name "")
if("lvgl" IN_LIST build_components)
    set(lvgl_name "lvgl")
elseif("lvgl__lvgl" IN_LIST build_components)
    set(lvgl_name "lvgl__lvgl")
endif()

if(NOT lvgl_name)
    set(lvgl_name "lvgl__lvgl")
endif()

set(lvgl_ver "")
if(DEFINED ENV{LVGL_VERSION} AND NOT "$ENV{LVGL_VERSION}" STREQUAL "")
    set(lvgl_ver $ENV{LVGL_VERSION})
else()
    idf_component_get_property(lvgl_ver ${lvgl_name} COMPONENT_VERSION)
endif()

if("${lvgl_ver}" STREQUAL "")
    set(lvgl_ver "9.0.0")
endif()

set(ADAPTER_SRCS
    "src/adapter/esp_lv_adapter.c"
    "src/display/display_manager.c"
    "src/display/bridge/common/display_bridge_common.c"
    "src/display/display_te_sync.c"
)

set(ADAPTER_PRIV_INCLUDES
    "src/adapter"
    "src/display"
    "src/display/ports"
    "src/display/bridge"
)

list(APPEND ADAPTER_PRIV_INCLUDES "src/input")

list(APPEND ADAPTER_PRIV_REQ espressif__esp_lcd_touch)
list(APPEND ADAPTER_SRCS
    "src/input/esp_lv_adapter_input_touch.c"
)

# Conditionally add button input support
if(CONFIG_ESP_LVGL_ADAPTER_ENABLE_BUTTON)
    list(APPEND ADAPTER_SRCS "src/input/esp_lv_adapter_input_button.c")
endif()

# Conditionally add knob/encoder input support
if(CONFIG_ESP_LVGL_ADAPTER_ENABLE_KNOB)
    list(APPEND ADAPTER_SRCS "src/input/esp_lv_adapter_input_knob.c")
endif()

if(lvgl_ver VERSION_LESS "9.0.0")
    list(APPEND ADAPTER_SRCS "src/display/bridge/v8/lvgl_bridge_v8.c")
    list(APPEND ADAPTER_SRCS "src/display/bridge/v8/lvgl_ppa_accel_v8.c")
    list(APPEND ADAPTER_PRIV_INCLUDES "src/display/bridge/v8")
else()
    list(APPEND ADAPTER_SRCS "src/display/bridge/v9/lvgl_bridge_v9.c")
    list(APPEND ADAPTER_SRCS "src/display/bridge/v9/lvgl_ppa_accel_v9.c")
    list(APPEND ADAPTER_PRIV_INCLUDES "src/display/bridge/v9")
endif()

set(ADAPTER_REQ esp_lcd ${lvgl_name})

# Conditionally add button component dependency
if(CONFIG_ESP_LVGL_ADAPTER_ENABLE_BUTTON)
    if("espressif__button" IN_LIST build_components)
        list(APPEND ADAPTER_REQ espressif__button)
    elseif("button" IN_LIST build_components)
        list(APPEND ADAPTER_REQ button)
    endif()
endif()

# Conditionally add knob component dependency
if(CONFIG_ESP_LVGL_ADAPTER_ENABLE_KNOB)
    if("espressif__knob" IN_LIST build_components)
        list(APPEND ADAPTER_REQ espressif__knob)
    elseif("knob" IN_LIST build_components)
        list(APPEND ADAPTER_REQ knob)
    endif()
endif()

if(CONFIG_ESP_LVGL_ADAPTER_ENABLE_DECODER)
    if("espressif__esp_lv_decoder" IN_LIST build_components)
        list(APPEND ADAPTER_REQ espressif__esp_lv_decoder)
    elseif("esp_lv_decoder" IN_LIST build_components)
        list(APPEND ADAPTER_REQ esp_lv_decoder)
    endif()
endif()

if(CONFIG_ESP_LVGL_ADAPTER_ENABLE_FS)
    if("espressif__esp_lv_fs" IN_LIST build_components)
        list(APPEND ADAPTER_REQ espressif__esp_lv_fs)
    elseif("esp_lv_fs" IN_LIST build_components)
        list(APPEND ADAPTER_REQ esp_lv_fs)
    endif()
endif()

if(CONFIG_ESP_LVGL_ADAPTER_ENABLE_FREETYPE)
    if("espressif__freetype" IN_LIST build_components)
        list(APPEND ADAPTER_REQ espressif__freetype)
    elseif("freetype" IN_LIST build_components)
        list(APPEND ADAPTER_REQ freetype)
    endif()
endif()

idf_component_register(
    SRCS ${ADAPTER_SRCS}
    INCLUDE_DIRS "include"
    PRIV_INCLUDE_DIRS ${ADAPTER_PRIV_INCLUDES}
    REQUIRES ${ADAPTER_REQ}
    PRIV_REQUIRES ${ADAPTER_PRIV_REQ}
)

idf_component_get_property(ESP_LCD_DIR esp_lcd COMPONENT_DIR)
if(ESP_LCD_DIR)
    target_include_directories(${COMPONENT_LIB} PRIVATE "${ESP_LCD_DIR}/priv_include")
endif()

target_link_libraries(${COMPONENT_LIB} PUBLIC idf::${lvgl_name})

set(ESP_LV_ADAPTER_PPA_ALIGNMENT 128)

target_compile_definitions(${COMPONENT_LIB} PUBLIC LVGL_PORT_PPA_ALIGNMENT=${ESP_LV_ADAPTER_PPA_ALIGNMENT})
target_compile_options(${COMPONENT_LIB} PUBLIC -include "${CMAKE_CURRENT_SOURCE_DIR}/src/display/ports/lvgl_port_alignment.h")

if("${lvgl_name}" IN_LIST build_components)
    idf_component_get_property(lvgl_comp_lib ${lvgl_name} COMPONENT_LIB)
    if(lvgl_comp_lib AND TARGET ${lvgl_comp_lib})
        target_compile_definitions(${lvgl_comp_lib} PUBLIC LVGL_PORT_PPA_ALIGNMENT=${ESP_LV_ADAPTER_PPA_ALIGNMENT})
        target_compile_options(${lvgl_comp_lib} PUBLIC -include "${CMAKE_CURRENT_SOURCE_DIR}/src/display/ports/lvgl_port_alignment.h")
        target_compile_options(${lvgl_comp_lib} PRIVATE -Wno-unused-function)
    endif()
endif()

if(CONFIG_ESP_LVGL_ADAPTER_ENABLE_FREETYPE)
    set(_freetype_component "")
    if("espressif__freetype" IN_LIST build_components)
        set(_freetype_component espressif__freetype)
    elseif("freetype" IN_LIST build_components)
        set(_freetype_component freetype)
    endif()

    if(NOT _freetype_component OR NOT lvgl_comp_lib)
        return()
    endif()

    if(NOT TARGET ${lvgl_comp_lib})
        return()
    endif()

    # Link FreeType library to LVGL
    target_link_libraries(${lvgl_comp_lib} PUBLIC idf::${_freetype_component})

    # Configure FreeType build definitions
    idf_build_set_property(COMPILE_DEFINITIONS "FT_ERR_PREFIX=FT_Err_" APPEND)
    idf_build_set_property(COMPILE_DEFINITIONS "FT_CONFIG_OPTION_ERROR_STRINGS" APPEND)
    idf_build_set_property(COMPILE_DEFINITIONS "FT2_BUILD_LIBRARY" APPEND)

    idf_component_get_property(freetype_comp_lib ${_freetype_component} COMPONENT_LIB)
    if(freetype_comp_lib AND TARGET ${freetype_comp_lib})
        target_compile_definitions(${freetype_comp_lib} INTERFACE
            FT_CONFIG_OPTION_ERROR_STRINGS
            FT2_BUILD_LIBRARY)
    endif()

    if(CONFIG_LV_FREETYPE_USE_LVGL_PORT AND
       TARGET freetype AND
       lvgl_ver VERSION_GREATER_EQUAL "9.0.0")

        set(_lvgl_dir "")
        if("${lvgl_name}" IN_LIST build_components)
            idf_component_get_property(_lvgl_dir ${lvgl_name} COMPONENT_DIR)
        endif()

        if(_lvgl_dir)
            set(_lv_ftsystem "${_lvgl_dir}/src/libs/freetype/lv_ftsystem.c")
        else()
            set(_lv_ftsystem "")
        endif()

        if(_lv_ftsystem AND EXISTS ${_lv_ftsystem})
            get_target_property(_ft_sources freetype SOURCES)
            list(FILTER _ft_sources EXCLUDE REGEX ".*/ftsystem\\.c$")
            set_target_properties(freetype PROPERTIES SOURCES "${_ft_sources}")

            target_sources(freetype PRIVATE ${_lv_ftsystem})
            target_include_directories(freetype PRIVATE ${_lvgl_dir})
        endif()
    endif()
endif()
