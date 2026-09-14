idf_component_register(
    SRCS "bq27220.c" "bq27220_data_memory.c"
    INCLUDE_DIRS "."
    PRIV_INCLUDE_DIRS "priv_include"
    REQUIRES driver
)
