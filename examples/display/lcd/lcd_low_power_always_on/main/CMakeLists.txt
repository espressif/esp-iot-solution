if(CONFIG_IDF_TARGET_ESP32P4)
    set(panel_src panel_dsi.c)
else()
    set(panel_src panel_rgb.c)
endif()
idf_component_register(SRCS main.c clock_face.c ${panel_src} INCLUDE_DIRS "."
                       PRIV_REQUIRES esp_pm esp_lcd esp_timer esp_driver_gpio esp_hw_support esp_psram)
