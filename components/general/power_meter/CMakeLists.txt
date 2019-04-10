
# componet standalone mode
if(NOT CONFIG_IOT_SOLUTION_EMBED)
    set(COMPONENT_SRCS "power_meter/power_meter.c"
                        "power_meter/power_meter_obj.cpp")

    set(COMPONENT_ADD_INCLUDEDIRS "power_meter/include")
else()
    if(CONFIG_IOT_POWER_METER_ENABLE)
        set(COMPONENT_SRCS "power_meter/power_meter.c"
                            "power_meter/power_meter_obj.cpp")

        set(COMPONENT_ADD_INCLUDEDIRS "power_meter/include")
    else()
        set(COMPONENT_SRCS "")
        set(COMPONENT_ADD_INCLUDEDIRS "")
        message(STATUS "Building empty power_meter component due to configuration")
    endif()
endif()

register_component()
