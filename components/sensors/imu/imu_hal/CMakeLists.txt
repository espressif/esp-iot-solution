set(requires bus mpu6050 lis2dh12)

message(STATUS "-----------IMU Info---------")
message(STATUS "requires = ${requires}")
message(STATUS "---------IMU Info End---------")

idf_component_register(SRCS "imu_hal.c"
                        INCLUDE_DIRS "include"
                        REQUIRES "${requires}")