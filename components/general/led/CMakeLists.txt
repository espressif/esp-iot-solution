
# componet standalone mode
if(NOT CONFIG_IOT_SOLUTION_EMBED)
    set(COMPONENT_SRCS "status_led/led.c"
                        "status_led/led_obj.cpp")

    set(COMPONENT_ADD_INCLUDEDIRS "status_led/include")
else()
    if(CONFIG_IOT_LED_ENABLE)
        set(COMPONENT_SRCS "status_led/led.c"
                            "status_led/led_obj.cpp")

        set(COMPONENT_ADD_INCLUDEDIRS "status_led/include")
    else()
        set(COMPONENT_SRCS "")
        set(COMPONENT_ADD_INCLUDEDIRS "")
        message(STATUS "Building empty led component due to configuration")
    endif()
endif()

# requirements can't depend on config
set(COMPONENT_REQUIRES base_class)

register_component()
