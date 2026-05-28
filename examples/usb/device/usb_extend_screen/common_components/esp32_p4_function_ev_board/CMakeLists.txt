set(SRCS "")
list(APPEND SRCS
    "src/bsp_board_extra.c"
    "src/bsp_sub_board.c"
    "src/esp32_p4_function_ev_board.c")

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

set(requires driver esp_driver_cam esp_lcd esp_driver_ppa esp_driver_isp)
if(${IDF_VERSION_MAJOR} GREATER_EQUAL 6)
    list(APPEND requires esp_driver_gpio esp_driver_i2c esp_driver_i2s esp_driver_ledc esp_driver_sdmmc)
endif()

idf_component_register(
    SRCS ${SRCS}
    INCLUDE_DIRS ${INCLUDE_DIRS}
    REQUIRES ${requires}
    PRIV_REQUIRES esp_timer spiffs fatfs esp_psram esp_mm
)

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
