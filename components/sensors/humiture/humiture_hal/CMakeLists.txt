set(requires bus sht3x hts221)

message(STATUS "-----------Humiture Info---------")
message(STATUS "requires = ${requires}")
message(STATUS "---------Humiture Info End---------")

idf_component_register(SRCS "humiture_hal.c"
                        INCLUDE_DIRS "include"
                        REQUIRES "${requires}")