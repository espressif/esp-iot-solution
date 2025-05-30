idf_component_register(SRCS "aht20_Sensor_Hub_Humiture.c"
                    INCLUDE_DIRS "."
                    PRIV_REQUIRES unity test_utils aht20 sensor_hub)
