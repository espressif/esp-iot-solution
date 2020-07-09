
set(COMPONENT_SRCS "led.c"
                    "led_obj.cpp")

set(COMPONENT_ADD_INCLUDEDIRS "include")


# requirements can't depend on config
set(COMPONENT_REQUIRES base_class)

register_component()
