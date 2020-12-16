set(requires bus mpu6050 lis2dh12 light_sensor_hal humiture_hal)

message(STATUS "-----------Sensor Hub Info---------")
message(STATUS "requires = ${requires}")
message(STATUS "---------Sensor Hub  Info End---------")

idf_component_register(SRC_DIRS "." "hal"
                        INCLUDE_DIRS "include"
                        REQUIRES "${requires}")