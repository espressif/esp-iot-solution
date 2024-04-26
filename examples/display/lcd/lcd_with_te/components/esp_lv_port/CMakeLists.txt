idf_component_register(SRCS "lv_port.c" INCLUDE_DIRS "include" REQUIRES "esp_lcd" PRIV_REQUIRES "esp_timer")

idf_build_get_property(build_components BUILD_COMPONENTS)
if("espressif__esp_lcd_touch" IN_LIST build_components)
    target_link_libraries(${COMPONENT_LIB} PRIVATE idf::espressif__esp_lcd_touch)
endif()
if("esp_lcd_touch" IN_LIST build_components)
    target_link_libraries(${COMPONENT_LIB} PRIVATE idf::esp_lcd_touch)
endif()
