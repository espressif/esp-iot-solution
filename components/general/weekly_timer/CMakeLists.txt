
# componet standalone mode
if(NOT CONFIG_IOT_SOLUTION_EMBED)
    set(COMPONENT_SRCS "weekly_timer.c")

    set(COMPONENT_ADD_INCLUDEDIRS "include")
else()
    if(CONFIG_IOT_WEEKLY_TIMER_ENABLE)
        set(COMPONENT_SRCS "weekly_timer.c")

        set(COMPONENT_ADD_INCLUDEDIRS "include")
    else()
        set(COMPONENT_SRCS "")
        set(COMPONENT_ADD_INCLUDEDIRS "")
        message(STATUS "Building empty weekly_timer component due to configuration")
    endif()
endif()

register_component()
