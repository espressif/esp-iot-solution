idf_component_register(SRCS "bh1750_test.c"
                        INCLUDE_DIRS "."
                        REQUIRES i2c_bus bh1750 unity)
