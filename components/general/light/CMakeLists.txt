
# componet standalone mode
if(NOT CONFIG_IOT_SOLUTION_EMBED)
    set(COMPONENT_SRCS "light.c"
                        "light_obj.cpp")

    set(COMPONENT_ADD_INCLUDEDIRS "include")
else()
    if(CONFIG_IOT_LIGHT_ENABLE)
        set(COMPONENT_SRCS "light.c"
                            "light_obj.cpp")

        set(COMPONENT_ADD_INCLUDEDIRS "include")
    else()
        set(COMPONENT_SRCS "")
        set(COMPONENT_ADD_INCLUDEDIRS "")
        message(STATUS "Building empty light component due to configuration")
    endif()
endif()

# requirements can't depend on config
set(COMPONENT_REQUIRES base_class)

register_component()
