idf_component_register(SRCS "bme280_test.c"
                        INCLUDE_DIRS .
                        REQUIRES i2c_bus unity bme280)
