
# componet standalone mode
if(NOT CONFIG_IOT_SOLUTION_EMBED)
    set(COMPONENT_SRCS "ota.c")

    set(COMPONENT_ADD_INCLUDEDIRS "include")
else()
    if(CONFIG_IOT_OTA_FUNC_ENABLE)
        set(COMPONENT_SRCS "ota.c")

        set(COMPONENT_ADD_INCLUDEDIRS "include")
    else()
        set(COMPONENT_SRCS "")
        set(COMPONENT_ADD_INCLUDEDIRS "")
        message(STATUS "Building empty ota component due to configuration")
    endif()
endif()

# requirements can't depend on config
set(COMPONENT_REQUIRES app_update)

register_component()
