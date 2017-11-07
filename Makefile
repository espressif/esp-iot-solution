#
# This is a project Makefile. It is assumed the directory this Makefile resides in is a
# project subdirectory.
#

#Overwrite the IDF_PATH to the esp-idf path in submodule.
IDF_PATH := $(IOT_SOLUTION_PATH)/submodule/esp-idf/
EXTRA_COMPONENT_DIRS += $(IOT_SOLUTION_PATH)/components/__config
EXTRA_COMPONENT_DIRS += $(IOT_SOLUTION_PATH)/components
EXTRA_COMPONENT_DIRS += $(IOT_SOLUTION_PATH)/components/features
EXTRA_COMPONENT_DIRS += $(IOT_SOLUTION_PATH)/components/general
EXTRA_COMPONENT_DIRS += $(IOT_SOLUTION_PATH)/components/wifi
EXTRA_COMPONENT_DIRS += $(IOT_SOLUTION_PATH)/components/spi_devices
EXTRA_COMPONENT_DIRS += $(IOT_SOLUTION_PATH)/components/i2c_devices
EXTRA_COMPONENT_DIRS += $(IOT_SOLUTION_PATH)/components/i2c_devices/sensor
EXTRA_COMPONENT_DIRS += $(IOT_SOLUTION_PATH)/components/i2c_devices/others
EXTRA_COMPONENT_DIRS += $(IOT_SOLUTION_PATH)/components/network
EXTRA_COMPONENT_DIRS += $(IOT_SOLUTION_PATH)/components/platforms
include $(IDF_PATH)/make/project.mk

