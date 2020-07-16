
set(COMPONENT_SRCS "wifi.c"
                    "wifi_obj.cpp")
set(COMPONENT_ADD_INCLUDEDIRS ". include")

# requirements can't depend on config
set(COMPONENT_REQUIRES nvs_flash)

register_component()
