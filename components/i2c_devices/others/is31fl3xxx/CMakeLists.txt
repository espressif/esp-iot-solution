
# componet standalone mode
if(NOT CONFIG_IOT_SOLUTION_EMBED)
    set(COMPONENT_SRCS "is31fl3736.c"
                        "is31fl3736_obj.cpp"
                        "is31fl3218.c"
                        "is31fl3218_obj.cpp")

    set(COMPONENT_ADD_INCLUDEDIRS ". include")
else()
    if(CONFIG_IOT_IS31FL3XXX_ENABLE)
        set(COMPONENT_SRCS "is31fl3736.c"
                            "is31fl3736_obj.cpp"
                            "is31fl3218.c"
                            "is31fl3218_obj.cpp")

        set(COMPONENT_ADD_INCLUDEDIRS ". include")
    else()
        set(COMPONENT_SRCS "")
        set(COMPONENT_ADD_INCLUDEDIRS "")
        message(STATUS "Building empty is31fl3xxx component due to configuration")
    endif()
endif()

# requirements can't depend on config
set(COMPONENT_REQUIRES i2c_bus)

register_component()
