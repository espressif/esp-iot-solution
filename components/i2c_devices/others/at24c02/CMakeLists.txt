idf_component_register(SRCS "at24c02.c" "at24c02_obj.cpp"
                        INCLUDE_DIRS include
                        PRIV_REQUIRES i2c_bus)