idf_component_register(
    SRCS "esp_lcd_usb_display.c"
    INCLUDE_DIRS "include"
    PRIV_REQUIRES usb_device_uvc
    REQUIRES esp_psram esp_lcd esp_driver_jpeg)

include(package_manager)
cu_pkg_define_version(${CMAKE_CURRENT_LIST_DIR})
