idf_component_register(SRCS "touch_button_sensor_main.c"
                       INCLUDE_DIRS "."
                       REQUIRES touch_button_sensor espressif__led_strip esp_timer)
