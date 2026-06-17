include($ENV{IDF_PATH}/tools/cmake/version.cmake)

set(ADAPTER_PRIV_REQ esp_timer esp_mm esp_pm)
idf_build_get_property(target IDF_TARGET)
if(${target} STREQUAL "esp32p4" OR ${target} STREQUAL "esp32s31")
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

idf_component_register(
    SRCS ${ADAPTER_SRCS}
    INCLUDE_DIRS "include"
    PRIV_INCLUDE_DIRS ${ADAPTER_PRIV_INCLUDES}
    REQUIRES ${ADAPTER_REQ}
    PRIV_REQUIRES ${ADAPTER_PRIV_REQ}
)

# Optional dependencies must be linked after registration. Keep both managed
# and local component names so projects can override registry components.
if(CONFIG_ESP_LVGL_ADAPTER_ENABLE_BUTTON)
    idf_component_optional_requires(PUBLIC espressif__button button)
endif()

if(CONFIG_ESP_LVGL_ADAPTER_ENABLE_KNOB)
    idf_component_optional_requires(PUBLIC espressif__knob knob)
endif()

if(CONFIG_ESP_LVGL_ADAPTER_ENABLE_DECODER)
    idf_component_optional_requires(PUBLIC espressif__esp_lv_decoder esp_lv_decoder)
endif()

if(CONFIG_ESP_LVGL_ADAPTER_ENABLE_FS)
    idf_component_optional_requires(PUBLIC espressif__esp_lv_fs esp_lv_fs)
endif()

if(CONFIG_ESP_LVGL_ADAPTER_ENABLE_FREETYPE)
    idf_component_optional_requires(PUBLIC espressif__freetype freetype)
endif()

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
    idf_component_get_property(lvgl_comp_dir ${lvgl_name} COMPONENT_DIR)
    if(lvgl_comp_lib AND TARGET ${lvgl_comp_lib})
        target_compile_definitions(${lvgl_comp_lib} PUBLIC LVGL_PORT_PPA_ALIGNMENT=${ESP_LV_ADAPTER_PPA_ALIGNMENT})
        target_compile_options(${lvgl_comp_lib} PUBLIC -include "${CMAKE_CURRENT_SOURCE_DIR}/src/display/ports/lvgl_port_alignment.h")
        target_compile_options(${lvgl_comp_lib} PRIVATE -Wno-unused-function)

        if(CONFIG_ESP_LVGL_ADAPTER_PARTIAL_AUX_IMG_CACHE)
            if(lvgl_ver VERSION_LESS "9.0.0")
                target_compile_definitions(${lvgl_comp_lib} PUBLIC LV_IMG_CACHE_DEF_SIZE=32)
            else()
                target_compile_definitions(${lvgl_comp_lib} PUBLIC LV_CACHE_DEF_SIZE=1048576 LV_IMAGE_HEADER_CACHE_DEF_CNT=32)
            endif()
        endif()

    endif()
endif()

function(esp_lv_adapter_target_compile_definitions target_name visibility)
    if("${target_name}" STREQUAL "")
        return()
    endif()

    if(NOT TARGET ${target_name})
        return()
    endif()

    get_target_property(_target_type ${target_name} TYPE)
    if(_target_type STREQUAL "INTERFACE_LIBRARY")
        target_compile_definitions(${target_name} INTERFACE ${ARGN})
    else()
        target_compile_definitions(${target_name} ${visibility} ${ARGN})
    endif()
endfunction()

function(esp_lv_adapter_ensure_lvgl_freetype_stack_check_hook)
    if(NOT lvgl_comp_dir)
        return()
    endif()

    set(_lv_conf_internal "${lvgl_comp_dir}/src/lv_conf_internal.h")

    if(NOT EXISTS "${_lv_conf_internal}")
        message(FATAL_ERROR "esp_lvgl_adapter: LVGL configuration file is missing")
    endif()

    file(READ "${_lv_conf_internal}" _lv_conf_internal_content)
    string(FIND "${_lv_conf_internal_content}" "LV_FREETYPE_SKIP_DRAW_STACK_CHECK" _lv_conf_hook_pos)

    if(NOT _lv_conf_hook_pos EQUAL -1)
        return()
    endif()

    set(_lv_stack_check [[    #if (LV_USE_FREETYPE || LV_USE_THORVG) && LV_DRAW_THREAD_STACK_SIZE < (32 * 1024)
        #error "Increase LV_DRAW_THREAD_STACK_SIZE to at least 32KB for FreeType or ThorVG."
    #endif]])
    set(_lv_stack_check_hook [[    #ifndef LV_FREETYPE_SKIP_DRAW_STACK_CHECK
        #define LV_FREETYPE_SKIP_DRAW_STACK_CHECK 0
    #endif
    #if !LV_FREETYPE_SKIP_DRAW_STACK_CHECK && (LV_USE_FREETYPE || LV_USE_THORVG) && LV_DRAW_THREAD_STACK_SIZE < (32 * 1024)
        #error "Increase LV_DRAW_THREAD_STACK_SIZE to at least 32KB for FreeType or ThorVG."
    #endif]])
    string(FIND "${_lv_conf_internal_content}" "${_lv_stack_check}" _lv_stack_check_pos)
    if(_lv_stack_check_pos EQUAL -1)
        message(FATAL_ERROR "esp_lvgl_adapter: unsupported LVGL FreeType stack check layout")
    endif()

    string(REPLACE "${_lv_stack_check}" "${_lv_stack_check_hook}" _lv_conf_internal_content "${_lv_conf_internal_content}")
    file(WRITE "${_lv_conf_internal}" "${_lv_conf_internal_content}")
    message(STATUS "esp_lvgl_adapter: configured LVGL FreeType stack check hook")
endfunction()

function(esp_lv_adapter_ensure_lvgl_freertos_stack_caps_hook)
    if(NOT lvgl_comp_dir)
        return()
    endif()

    set(_lv_freertos "${lvgl_comp_dir}/src/osal/lv_freertos.c")

    if(NOT EXISTS "${_lv_freertos}")
        message(FATAL_ERROR "esp_lvgl_adapter: LVGL FreeRTOS backend file is missing")
    endif()

    file(READ "${_lv_freertos}" _lv_freertos_content)
    string(FIND "${_lv_freertos_content}" "#include \"freertos/idf_additions.h\"" _lv_freertos_include_hook_pos)
    string(FIND "${_lv_freertos_content}" "xTaskCreateWithCaps(" _lv_thread_create_hook_pos)
    string(FIND "${_lv_freertos_content}" "vTaskDeleteWithCaps(pxThread->xTaskHandle)" _lv_thread_delete_hook_pos)

    set(_lv_freertos_hook_ready OFF)
    if(NOT _lv_freertos_include_hook_pos EQUAL -1 AND
       NOT _lv_thread_create_hook_pos EQUAL -1 AND
       NOT _lv_thread_delete_hook_pos EQUAL -1)
        set(_lv_freertos_hook_ready ON)
    endif()

    set(_lv_freertos_hook_partial OFF)
    if(NOT _lv_freertos_include_hook_pos EQUAL -1 OR
       NOT _lv_thread_create_hook_pos EQUAL -1 OR
       NOT _lv_thread_delete_hook_pos EQUAL -1)
        set(_lv_freertos_hook_partial ON)
    endif()

    if(_lv_freertos_hook_ready)
        return()
    elseif(_lv_freertos_hook_partial)
        message(FATAL_ERROR "esp_lvgl_adapter: partially configured LVGL FreeRTOS stack caps hook")
    endif()

    set(_lv_freertos_include "#include \"atomic.h\"")
    set(_lv_freertos_include_hook "#include \"atomic.h\"\n#if defined(ESP_PLATFORM) && defined(LV_FREERTOS_TASK_STACK_ALLOC_CAPS)\n    #include \"freertos/idf_additions.h\"\n#endif")
    set(_lv_thread_create [[    BaseType_t xTaskCreateStatus = xTaskCreate(
                                       prvRunThread,
                                       name,
                                       (configSTACK_DEPTH_TYPE)(usStackSize / sizeof(StackType_t)),
                                       (void *)pxThread,
                                       tskIDLE_PRIORITY + xSchedPriority,
                                       &pxThread->xTaskHandle);]])
    set(_lv_thread_create_hook [[#if defined(ESP_PLATFORM) && defined(LV_FREERTOS_TASK_STACK_ALLOC_CAPS)
    BaseType_t xTaskCreateStatus = xTaskCreateWithCaps(
                                       prvRunThread,
                                       name,
                                       (configSTACK_DEPTH_TYPE)usStackSize,
                                       (void *)pxThread,
                                       tskIDLE_PRIORITY + xSchedPriority,
                                       &pxThread->xTaskHandle,
                                       LV_FREERTOS_TASK_STACK_ALLOC_CAPS);
#else
    BaseType_t xTaskCreateStatus = xTaskCreate(
                                       prvRunThread,
                                       name,
                                       (configSTACK_DEPTH_TYPE)(usStackSize / sizeof(StackType_t)),
                                       (void *)pxThread,
                                       tskIDLE_PRIORITY + xSchedPriority,
                                       &pxThread->xTaskHandle);
#endif]])
    set(_lv_thread_delete "    vTaskDelete(pxThread->xTaskHandle);")
    set(_lv_thread_delete_hook [[#if defined(ESP_PLATFORM) && defined(LV_FREERTOS_TASK_STACK_ALLOC_CAPS)
    vTaskDeleteWithCaps(pxThread->xTaskHandle);
#else
    vTaskDelete(pxThread->xTaskHandle);
#endif]])

    foreach(_lv_required_snippet IN ITEMS _lv_freertos_include _lv_thread_create _lv_thread_delete)
        string(FIND "${_lv_freertos_content}" "${${_lv_required_snippet}}" _lv_snippet_pos)
        if(_lv_snippet_pos EQUAL -1)
            message(FATAL_ERROR "esp_lvgl_adapter: unsupported LVGL FreeRTOS stack hook layout")
        endif()
    endforeach()

    string(REPLACE "${_lv_freertos_include}" "${_lv_freertos_include_hook}" _lv_freertos_content "${_lv_freertos_content}")
    string(REPLACE "${_lv_thread_create}" "${_lv_thread_create_hook}" _lv_freertos_content "${_lv_freertos_content}")
    string(REPLACE "${_lv_thread_delete}" "${_lv_thread_delete_hook}" _lv_freertos_content "${_lv_freertos_content}")
    file(WRITE "${_lv_freertos}" "${_lv_freertos_content}")
    message(STATUS "esp_lvgl_adapter: configured LVGL FreeRTOS stack caps hook")
endfunction()

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
    if(freetype_comp_lib)
        esp_lv_adapter_target_compile_definitions(${freetype_comp_lib} INTERFACE
            FT_CONFIG_OPTION_ERROR_STRINGS
            FT2_BUILD_LIBRARY)
    endif()

    if((CONFIG_ESP_LVGL_ADAPTER_FREETYPE_SMALL_RENDER_POOL OR
        CONFIG_ESP_LVGL_ADAPTER_FREETYPE_MINIMAL_BUILD) AND TARGET freetype)
        target_include_directories(freetype PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/src/freetype")
        target_compile_definitions(freetype PRIVATE
            "FT_CONFIG_OPTIONS_H=<esp_lv_adapter_freetype_ftoption.h>")
    endif()

    if(CONFIG_ESP_LVGL_ADAPTER_FREETYPE_MINIMAL_BUILD AND TARGET freetype)
        target_compile_definitions(freetype PRIVATE
            "FT_CONFIG_MODULES_H=<esp_lv_adapter_freetype_ftmodule.h>")
    endif()

    if(CONFIG_ESP_LVGL_ADAPTER_FREETYPE_SMALL_RENDER_POOL AND
       lvgl_ver VERSION_GREATER_EQUAL "9.0.0" AND
       lvgl_comp_lib AND TARGET ${lvgl_comp_lib})
        esp_lv_adapter_ensure_lvgl_freetype_stack_check_hook()
        esp_lv_adapter_target_compile_definitions(${COMPONENT_LIB} PUBLIC LV_FREETYPE_SKIP_DRAW_STACK_CHECK=1)
        esp_lv_adapter_target_compile_definitions(${lvgl_comp_lib} PUBLIC LV_FREETYPE_SKIP_DRAW_STACK_CHECK=1)
        esp_lv_adapter_target_compile_definitions(freetype PRIVATE LV_FREETYPE_SKIP_DRAW_STACK_CHECK=1)
        if(freetype_comp_lib)
            esp_lv_adapter_target_compile_definitions(${freetype_comp_lib} PRIVATE LV_FREETYPE_SKIP_DRAW_STACK_CHECK=1)
        endif()
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

if(CONFIG_ESP_LVGL_ADAPTER_LVGL_THREAD_STACK_IN_PSRAM AND
   lvgl_ver VERSION_GREATER_EQUAL "9.0.0" AND
   lvgl_comp_lib AND TARGET ${lvgl_comp_lib})
    esp_lv_adapter_ensure_lvgl_freertos_stack_caps_hook()
    esp_lv_adapter_target_compile_definitions(${lvgl_comp_lib} PRIVATE
        "LV_FREERTOS_TASK_STACK_ALLOC_CAPS=(MALLOC_CAP_SPIRAM|MALLOC_CAP_8BIT)")
endif()
