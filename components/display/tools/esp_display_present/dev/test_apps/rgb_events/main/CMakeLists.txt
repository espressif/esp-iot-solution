set(PRESENT_BOARD "$ENV{PRESENT_TEST_PRESENT_BOARD}")
if(NOT IDF_TARGET STREQUAL "esp32s31" OR NOT PRESENT_BOARD STREQUAL "s31-rgb")
    message(FATAL_ERROR "RGB event probe requires confirmed S31 Korvo stock RGB")
endif()
idf_component_register(SRCS main.c "../../boards/main/board_s31_rgb.c"
    PRIV_INCLUDE_DIRS "../../boards/main"
    REQUIRES esp_lcd esp_timer esp_psram esp_driver_gpio esp_mm)
target_compile_definitions(${COMPONENT_LIB} PRIVATE PRESENT_BOARD_S31_RGB=1)
