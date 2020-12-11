set(requires bus imu_hal lightsensor_hal humiture_hal)

message(STATUS "-----------Sensor Hub Info---------")
message(STATUS "requires = ${requires}")
message(STATUS "---------Sensor Hub  Info End---------")

idf_component_register(SRCS "sensors.c" "sensors_event.c"
                        INCLUDE_DIRS include
                        REQUIRES "${requires}")