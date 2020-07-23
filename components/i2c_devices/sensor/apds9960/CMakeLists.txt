idf_component_register(SRCS "apds9960.c" "apds9960_obj.cpp"
                        INCLUDE_DIRS include
                        REQUIRES i2c_bus)