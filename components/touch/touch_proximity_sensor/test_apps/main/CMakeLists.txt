idf_component_register(SRCS "touch_proximity_sensor_test.c"
                    INCLUDE_DIRS "."
                    PRIV_REQUIRES unity touch_proximity_sensor touch_sensor_lowlevel
                    WHOLE_ARCHIVE)
