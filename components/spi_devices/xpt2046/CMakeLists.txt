
# componet standalone mode
if(NOT CONFIG_IOT_SOLUTION_EMBED)
    set(COMPONENT_SRCS "iot_xpt.c"
                        "xpt2046_obj.cpp")
    set(COMPONENT_ADD_INCLUDEDIRS ". include")
else()
    if(CONFIG_IOT_TOUCH_SCREEN_ENABLE)
        set(COMPONENT_SRCS "iot_xpt.c"
                            "xpt2046_obj.cpp")
        set(COMPONENT_ADD_INCLUDEDIRS ". include")
    else()
        set(COMPONENT_SRCS "")
        set(COMPONENT_ADD_INCLUDEDIRS "")
        message(STATUS "Building empty xpt2046 component due to configuration")
    endif()
endif()

register_component()
