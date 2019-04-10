
# componet standalone mode
if(NOT CONFIG_IOT_SOLUTION_EMBED)
    set(COMPONENT_SRCS "wifi.c"
                        "wifi_obj.cpp")
    set(COMPONENT_ADD_INCLUDEDIRS ". include")
else()
    if(CONFIG_IOT_WIFI_ABSTRACT_ENABLE)
        set(COMPONENT_SRCS "wifi.c"
                            "wifi_obj.cpp")
        set(COMPONENT_ADD_INCLUDEDIRS ". include")
    else()
        set(COMPONENT_SRCS "")
        set(COMPONENT_ADD_INCLUDEDIRS "")
        message(STATUS "Building empty wifi_conn component due to configuration")
    endif()
endif()

# requirements can't depend on config
set(COMPONENT_REQUIRES nvs_flash)

register_component()
