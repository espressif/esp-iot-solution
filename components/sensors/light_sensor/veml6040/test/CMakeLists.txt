idf_component_register(SRCS "veml6040_test.c"
                        INCLUDE_DIRS "."
                        REQUIRES i2c_bus veml6040 unity)
