idf_component_register(SRCS "hdc2010.c" "hdc2010_obj.cpp"
                        INCLUDE_DIRS include
                        PRIV_REQUIRES i2c_bus)