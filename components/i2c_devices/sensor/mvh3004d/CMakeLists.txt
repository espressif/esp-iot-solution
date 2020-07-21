idf_component_register(SRCS "mvh3004d.c" "mvh3004d_obj.cpp"
                        INCLUDE_DIRS include
                        PRIV_REQUIRES i2c_bus)