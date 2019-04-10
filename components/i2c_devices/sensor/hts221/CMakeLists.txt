
# componet standalone mode
if(NOT CONFIG_IOT_SOLUTION_EMBED)
    set(COMPONENT_SRCS "hts221.c"
                        "hts221_obj.cpp")

    set(COMPONENT_ADD_INCLUDEDIRS ". include")
else()
    if(CONFIG_IOT_HTS221_ENABLE)
        set(COMPONENT_SRCS "hts221.c"
                            "hts221_obj.cpp")

        set(COMPONENT_ADD_INCLUDEDIRS ". include")
    else()
        set(COMPONENT_SRCS "")
        set(COMPONENT_ADD_INCLUDEDIRS "")
        message(STATUS "Building empty hts221 component due to configuration")
    endif()
endif()

# requirements can't depend on config
set(COMPONENT_REQUIRES i2c_bus)

register_component()
