idf_component_register(SRCS "mvh3004d_test.c"
                        INCLUDE_DIRS .
                        REQUIRES unity mvh3004d i2c_bus)
