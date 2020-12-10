set(requires bus bh1750 veml6040 veml6075)

message(STATUS "-----------Lightsensor Info---------")
message(STATUS "requires = ${requires}")
message(STATUS "---------Lightsensor Info End---------")

idf_component_register(SRCS "light_sensor_hal.c"
                        INCLUDE_DIRS "include"
                        REQUIRES "${requires}")