
set(COMPONENT_SRCS "lowpower_evb_callback.cpp")

set(COMPONENT_ADD_INCLUDEDIRS ". include")

set(COMPONENT_REQUIRES wifi_conn tcp smart_config lowpower_framework lowpower_evb_peripherals lowpower_evb_ulp lowpower_evb_wifi)

register_component()
