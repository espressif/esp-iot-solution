
# componet standalone mode
if(NOT CONFIG_IOT_SOLUTION_EMBED)
    set(COMPONENT_SRCS "ulp_monitor.c")

    set(COMPONENT_ADD_INCLUDEDIRS "include")
else()
    if(CONFIG_IOT_ULP_ENABLE)
        set(COMPONENT_SRCS "ulp_monitor.c")

        set(COMPONENT_ADD_INCLUDEDIRS "include")
    else()
        set(COMPONENT_SRCS "")
        set(COMPONENT_ADD_INCLUDEDIRS "")
        message(STATUS "Building empty ulp component due to configuration")
    endif()
endif()

# requirements can't depend on config
set(COMPONENT_REQUIRES ulp)

register_component()
