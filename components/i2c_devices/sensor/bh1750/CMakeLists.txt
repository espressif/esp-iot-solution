idf_component_register(SRCS "bh1750.c" "bh1750_obj.cpp"
                        INCLUDE_DIRS include
                        REQUIRES i2c_bus)