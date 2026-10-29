idf_component_register(
    SRCS "main.c" "lcd_init.c" "lcd_pattern_test.c"
    INCLUDE_DIRS "."
    REQUIRES esp_lcd)
