idf_component_register(
    SRCS "src/esp32_s31_korvo_1.c" "src/bsp_board_extra.c"
    INCLUDE_DIRS "include"
    REQUIRES esp_driver_gpio esp_driver_i2c esp_driver_i2s esp_lcd
)
