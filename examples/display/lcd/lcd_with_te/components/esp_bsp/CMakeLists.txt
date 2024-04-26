idf_component_register(
    SRCS "esp_bsp.c"
    INCLUDE_DIRS "include"
    PRIV_INCLUDE_DIRS "priv_include"
    REQUIRES driver
    PRIV_REQUIRES esp_lcd esp_lv_port
)
