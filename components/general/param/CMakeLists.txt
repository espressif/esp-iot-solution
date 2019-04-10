
# componet standalone mode
if(NOT CONFIG_IOT_SOLUTION_EMBED)
    set(COMPONENT_SRCS "param.c")

    set(COMPONENT_ADD_INCLUDEDIRS "include")
else()
    if(CONFIG_IOT_PARAM_ENABLE)
        set(COMPONENT_SRCS "param.c")

        set(COMPONENT_ADD_INCLUDEDIRS "include")
    else()
        set(COMPONENT_SRCS "")
        set(COMPONENT_ADD_INCLUDEDIRS "")
        message(STATUS "Building empty param component due to configuration")
    endif()
endif()

# requirements can't depend on config
set(COMPONENT_REQUIRES nvs_flash)

register_component()
