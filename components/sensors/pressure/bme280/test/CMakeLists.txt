idf_component_register(SRCS "bme280_test.c"
                        INCLUDE_DIRS .
                        REQUIRES bus unity bme280)