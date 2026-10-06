idf_component_register(
    SRCS "bmm350.c" "bmm350_api.c"
    INCLUDE_DIRS "include"
    REQUIRES i2c_bus esp_timer
)
