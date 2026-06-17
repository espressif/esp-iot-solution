idf_component_register(
    SRCS "esp32_p4_function_ev_board.c"
    INCLUDE_DIRS "include"
    PRIV_INCLUDE_DIRS "priv_include"
    REQUIRES esp_driver_gpio esp_driver_i2c esp_lcd esp_lvgl_adapter
    PRIV_REQUIRES esp_driver_ledc esp_psram
)
