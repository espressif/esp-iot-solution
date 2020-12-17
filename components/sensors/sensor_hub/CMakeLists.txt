set(requires bus mpu6050 lis2dh12 sht3x hts221 bh1750 veml6040 veml6075)

message(STATUS "-----------Sensor Hub Info---------")
message(STATUS "requires = ${requires}")
message(STATUS "---------Sensor Hub  Info End---------")

idf_component_register(SRC_DIRS "." "hal"
                        INCLUDE_DIRS "include"
                        REQUIRES "${requires}")