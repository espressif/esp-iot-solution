idf_component_register(
    SRCS "bmm350.c" "bmm350_oor.c" "common/bmm350_common.c"
    INCLUDE_DIRS "." "common"
    REQUIRES "i2c_bus" "esp_timer" "esp_driver_i2c"
)
