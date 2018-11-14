
set(COMPONENT_SRCS "plug_device/plug_device.c"
                    "light_device/light_device.c")

set(COMPONENT_ADD_INCLUDEDIRS "plug_device/include"
                                "light_device/include")

set(COMPONENT_REQUIRES touchpad button led light power_meter param relay)

register_component()
