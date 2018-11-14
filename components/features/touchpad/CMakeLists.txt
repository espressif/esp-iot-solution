
# componet standalone mode
if(NOT CONFIG_IOT_SOLUTION_EMBED)
    set(COMPONENT_SRCS "touchpad_obj.cpp"
                        "touchpad.c")

    set(COMPONENT_ADD_INCLUDEDIRS "include")

    if(CONFIG_IOT_TOUCH_ENABLE)
        set(COMPONENT_SRCS "${COMPONENT_SRCS}"
                            "scope_debug/touch_tune_tool.c")

        set(COMPONENT_ADD_INCLUDEDIRS "${COMPONENT_ADD_INCLUDEDIRS}" 
                                        "scope_debug")
    endif()
else()
    if(CONFIG_IOT_TOUCH_ENABLE)
        set(COMPONENT_SRCS "touchpad_obj.cpp"
                            "touchpad.c")

        set(COMPONENT_ADD_INCLUDEDIRS "include")

    if(CONFIG_IOT_TOUCH_ENABLE)
        set(COMPONENT_SRCS "${COMPONENT_SRCS}"
                            "scope_debug/touch_tune_tool.c")

        set(COMPONENT_ADD_INCLUDEDIRS "${COMPONENT_ADD_INCLUDEDIRS}"
                                        "scope_debug")
    endif()

    else()
        set(COMPONENT_SRCS "")
        set(COMPONENT_ADD_INCLUDEDIRS "")
        message(STATUS "Building empty infrared component due to configuration")
    endif()
endif()

# requirements can't depend on config
set(COMPONENT_REQUIRES nvs_flash)

register_component()
