
# componet standalone mode
if(NOT CONFIG_IOT_SOLUTION_EMBED)
    set(COMPONENT_SRCS "lowpower_framework.c")

    set(COMPONENT_ADD_INCLUDEDIRS "include")
else()
    if(CONFIG_IOT_LOWPOWER_APP_FRAMEWORK_ENABLE)
        set(COMPONENT_SRCS "lowpower_framework.c")

        set(COMPONENT_ADD_INCLUDEDIRS "include")
    else()
        set(COMPONENT_SRCS "")
        set(COMPONENT_ADD_INCLUDEDIRS "")
        message(STATUS "Building empty lowpower_framework component due to configuration")
    endif()
endif()

register_component()
