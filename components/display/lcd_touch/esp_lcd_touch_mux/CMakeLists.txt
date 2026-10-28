if("${IDF_VERSION_MAJOR}.${IDF_VERSION_MINOR}" VERSION_GREATER_EQUAL "5.3")
    set(DRIVER_REQUIRES esp_driver_gpio esp_driver_gptimer)
else()
    set(DRIVER_REQUIRES driver)
endif()

idf_component_register(
    SRCS
        "esp_lcd_touch_mux.c"
    INCLUDE_DIRS
        "include"
    REQUIRES
        esp_lcd_touch
        ${DRIVER_REQUIRES}
        freertos
)
