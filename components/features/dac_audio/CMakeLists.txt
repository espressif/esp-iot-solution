
# componet standalone mode
if(NOT CONFIG_IOT_SOLUTION_EMBED)
    set(COMPONENT_SRCS "dac_audio.c")

    set(COMPONENT_ADD_INCLUDEDIRS "include")
else()
    if(CONFIG_IOT_DAC_AUDIO_ENABLE)
        set(COMPONENT_SRCS "dac_audio.c")

        set(COMPONENT_ADD_INCLUDEDIRS "include")
    else()
        set(COMPONENT_SRCS "")
        set(COMPONENT_ADD_INCLUDEDIRS "")
        message(STATUS "Building empty dac_audio component due to configuration")
    endif()
endif()

register_component()
