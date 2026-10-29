set(srcs "main.c" "app_video.c" "app_lcd.c")

idf_component_register(SRCS "${srcs}"
                       PRIV_INCLUDE_DIRS .
                       PRIV_REQUIRES esp_driver_ppa esp_mm esp_timer esp_lcd)

idf_component_get_property(_lcd_dir esp_lcd COMPONENT_DIR)
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${_lcd_dir}/dsi/include/esp_lcd_mipi_dsi.h")
file(STRINGS "${_lcd_dir}/dsi/include/esp_lcd_mipi_dsi.h" _frame_callback
     REGEX "[ \t]on_frame_buf_complete[ \t]*;")
if(NOT _frame_callback)
    message(FATAL_ERROR "video_lcd_display requires the native DPI framebuffer-completion callback; update to IDF 5.5.6 or a current 6.x release")
endif()
