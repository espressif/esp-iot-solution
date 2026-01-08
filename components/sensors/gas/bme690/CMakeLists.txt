idf_component_register(
    SRCS "bme69x.c" "common/bme690_common.c"
    INCLUDE_DIRS "." "common"
    REQUIRES "esp_driver_i2c" "esp_driver_spi"
)
