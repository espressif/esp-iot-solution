
# componet standalone mode
if(NOT CONFIG_IOT_SOLUTION_EMBED)
    set(COMPONENT_SRCS "adc/adc.c"
                        "adc/adc_obj.cpp")

    set(COMPONENT_ADD_INCLUDEDIRS "adc/include")
else()
    if(CONFIG_IOT_ADC_ENABLE)
        set(COMPONENT_SRCS "adc/adc.c"
                            "adc/adc_obj.cpp")

        set(COMPONENT_ADD_INCLUDEDIRS "adc/include")
    else()
        set(COMPONENT_SRCS "")
        set(COMPONENT_ADD_INCLUDEDIRS "")
        message(STATUS "Building empty adc component due to configuration")
    endif()
endif()

# requirements can't depend on config
set(COMPONENT_REQUIRES esp_adc_cal)

register_component()
