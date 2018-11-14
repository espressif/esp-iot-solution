
set(COMPONENT_SRCS "cloud/cloud.c"
                    "device/device.c"
                    "parse/parse.c")

set(COMPONENT_ADD_INCLUDEDIRS "cloud/include"
                                "device/include"
                                "parse/include")

set(COMPONENT_REQUIRES alink button light products)

register_component()
