idf_component_register(SRCS "veml6040.c" "veml6040_obj.cpp"
                        INCLUDE_DIRS include
                        PRIV_REQUIRES i2c_bus)