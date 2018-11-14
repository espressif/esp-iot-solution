
set(COMPONENT_SRCS "alink_key_trigger.c"
                    "main.c")

set(COMPONENT_ADD_INCLUDEDIRS ".")

set(COMPONENT_REQUIRES alink touchpad button ssd1306 apds9960 hts221 wifi_conn abstract products)

register_component()
