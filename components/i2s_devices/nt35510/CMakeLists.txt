
# componet standalone mode
if(NOT CONFIG_IOT_SOLUTION_EMBED)
    set(COMPONENT_SRCS "nt35510.c")

    set(COMPONENT_ADD_INCLUDEDIRS ". include")
else()
    if(CONFIG_IOT_LCD_NT35510_ENABLE)
        set(COMPONENT_SRCS "nt35510.c")

        set(COMPONENT_ADD_INCLUDEDIRS ". include")
    else()
        set(COMPONENT_SRCS "")
        set(COMPONENT_ADD_INCLUDEDIRS "")
        message(STATUS "Building empty nt35510 component due to configuration")
    endif()
endif()

# requirements can't depend on config
set(COMPONENT_REQUIRES lcd_common)

register_component()
