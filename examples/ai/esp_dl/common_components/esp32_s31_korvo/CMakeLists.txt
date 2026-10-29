idf_component_register(
    SRCS "src/esp32_s31_korvo.c"
    INCLUDE_DIRS "include"
    REQUIRES esp_driver_gpio esp_driver_i2c esp_driver_cam esp_lcd esp_lvgl_adapter espressif__esp_lcd_touch_gt1151 esp_video
)
