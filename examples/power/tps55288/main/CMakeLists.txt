idf_component_register(SRCS "tps55288_example_main.c"
                       INCLUDE_DIRS "."
                       PRIV_REQUIRES tps55288 esp_driver_i2c)
