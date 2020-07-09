
set(COMPONENT_SRCS "app_oled.cpp"
                    "app_power.cpp"
                    "main.cpp")

set(COMPONENT_ADD_INCLUDEDIRS ". include")

set(COMPONENT_REQUIRES  mbedtls touchpad ssd1306 apds9960 hts221 wifi_conn)

register_component()
