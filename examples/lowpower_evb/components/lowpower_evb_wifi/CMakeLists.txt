
set(COMPONENT_SRCS "lowpower_evb_wifi.cpp")

set(COMPONENT_ADD_INCLUDEDIRS ". include")

set(COMPONENT_REQUIRES lowpower_framework smart_config button param)

register_component()
