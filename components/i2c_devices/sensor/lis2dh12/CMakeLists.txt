idf_component_register(SRCS "lis2dh12.c" "lis2dh12_obj.cpp"
                        INCLUDE_DIRS include
                        PRIV_REQUIRES i2c_bus)