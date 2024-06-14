idf_component_register(SRCS "sht3x.c"
                        INCLUDE_DIRS include
                        REQUIRES i2c_bus esp_timer)
