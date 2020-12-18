#common requires
set(requires bus)
#imu sensor requires
list(APPEND requires mpu6050 lis2dh12)
#humiture sensor requires
list(APPEND requires sht3x hts221)
#light sensor requires
list(APPEND requires bh1750 veml6040 veml6075)

idf_component_register(SRC_DIRS "." "hal"
                        INCLUDE_DIRS "include"
                        REQUIRES "${requires}")