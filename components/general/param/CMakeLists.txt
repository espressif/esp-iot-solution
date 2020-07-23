idf_component_register(SRCS "param.c"
                        INCLUDE_DIRS include
                        PRIV_REQUIRES spi_flash nvs_flash)