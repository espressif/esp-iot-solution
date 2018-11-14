
# componet standalone mode
if(NOT CONFIG_IOT_SOLUTION_EMBED)
    set(COMPONENT_SRCS "epaper.c"
                        "epaper_font.c")
    set(COMPONENT_ADD_INCLUDEDIRS ". include")
else()
    if(CONFIG_IOT_EINK_ENABLE)
        set(COMPONENT_SRCS "epaper.c"
                            "epaper_font.c")
        set(COMPONENT_ADD_INCLUDEDIRS ". include")
    else()
        set(COMPONENT_SRCS "")
        set(COMPONENT_ADD_INCLUDEDIRS "")
        message(STATUS "Building empty epaper component due to configuration")
    endif()
endif()

register_component()
