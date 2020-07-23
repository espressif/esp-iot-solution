idf_component_register(SRCS "bme280.c" "bme280_obj.cpp"
                        INCLUDE_DIRS include
                        REQUIRES i2c_bus)