idf_component_register(SRCS "ssd1306.c" "ssd1306_obj.cpp" "ssd1306_fonts.c"
                        INCLUDE_DIRS include
                        REQUIRES i2c_bus)