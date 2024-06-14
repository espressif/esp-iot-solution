idf_component_register(SRCS "at24c02_test.c"
                        INCLUDE_DIRS .
                        REQUIRES i2c_bus unity at24c02)
