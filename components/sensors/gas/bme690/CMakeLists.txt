idf_component_register(
    SRCS "bme69x.c" "common/bme690_common.c"
    INCLUDE_DIRS "." "common"
    REQUIRES "i2c_bus" "esp_driver_spi" "esp_timer"
)
