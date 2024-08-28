set(SRCS "")
list(APPEND SRCS
    "src/bsp_board_extra.c"
    "src/bsp_sub_board.c"
    "src/esp32_p4_function_ev_board.c")
if(CONFIG_BSP_DISPLAY_LVGL_VERSION_V8)
    list(APPEND SRCS "src/bsp_lvgl_port_v8.c")
elseif(CONFIG_BSP_DISPLAY_LVGL_VERSION_V9)
    list(APPEND SRCS "src/bsp_lvgl_port_v9.c" "src/lv_esp_freertos.c")
endif()

set(INCLUDE_DIRS "")
list(APPEND INCLUDE_DIRS "include")
list(APPEND INCLUDE_DIRS "priv_include")

set(SRC_DIRS "")
list(APPEND SRC_DIRS "src/sensor")

foreach(SRC_DIR IN LISTS SRC_DIRS)
    file(GLOB_RECURSE SRC ${SRC_DIR}/*.c)
    list(APPEND SRCS ${SRC})
    list(APPEND INCLUDE_DIRS ${SRC_DIR}/include)
endforeach()

idf_component_register(
    SRCS ${SRCS}
    INCLUDE_DIRS ${INCLUDE_DIRS}
    REQUIRES driver esp_driver_cam esp_lcd esp_driver_ppa esp_driver_isp
    PRIV_REQUIRES esp_timer spiffs fatfs esp_psram esp_mm
)

if(CONFIG_BSP_DISPLAY_LVGL_VERSION_V9)
    if (CONFIG_LV_OS_CUSTOM)
        idf_component_get_property(lvgl_lib lvgl__lvgl COMPONENT_LIB)
        target_link_libraries(${lvgl_lib} PUBLIC ${COMPONENT_LIB})
    endif()
endif()

target_compile_options(
    ${COMPONENT_LIB} PRIVATE
        -Wno-format
        -Wno-int-conversion
        -Wno-incompatible-pointer-types
        -Wunused-function
        -Wno-unused-variable
        -Wno-unused-function
        -Wno-overflow
        -Wno-unused-but-set-variable
        -Wno-discarded-qualifiers)
