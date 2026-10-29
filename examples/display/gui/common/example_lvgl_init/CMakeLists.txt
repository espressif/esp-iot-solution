idf_component_register(
    SRCS "example_lvgl_init.c"
    INCLUDE_DIRS "."
    REQUIRES hw_init esp_lvgl_adapter
)
