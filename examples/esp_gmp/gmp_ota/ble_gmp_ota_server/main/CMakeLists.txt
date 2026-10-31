idf_component_register(
    SRCS "main.c"
    INCLUDE_DIRS "."
    REQUIRES nvs_flash freertos bt esp_gmp esp_flux ble_conn_mgr gmp_ota_common
)
