#
# This is a project Makefile. It is assumed the directory this Makefile resides in is a
# project subdirectory.
#

PROJECT_NAME := touch_pad_evb

#If IOT_SOLUTION_PATH is not defined, use relative path as default value
IOT_SOLUTION_PATH ?= $(abspath $(shell pwd)/../../)

EXTRA_COMPONENT_DIRS += $(IOT_SOLUTION_PATH)/components/features/touchpad
EXTRA_COMPONENT_DIRS += $(IOT_SOLUTION_PATH)/components/general/led
EXTRA_COMPONENT_DIRS += $(IOT_SOLUTION_PATH)/components/general/light
EXTRA_COMPONENT_DIRS += $(IOT_SOLUTION_PATH)/components/i2c_devices/i2c_bus
EXTRA_COMPONENT_DIRS += $(IOT_SOLUTION_PATH)/components/i2c_devices/others/ch450

include $(IDF_PATH)/make/project.mk