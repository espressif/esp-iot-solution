idf_component_register(SRCS "param.c"
                        INCLUDE_DIRS include
                        REQUIRES spi_flash nvs_flash)