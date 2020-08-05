#
# This is a project Makefile. It is assumed the directory this Makefile resides in is a
# project subdirectory.
#

PROJECT_NAME := ugfx_example

#If IOT_SOLUTION_PATH is not defined, use relative path as default value
IOT_SOLUTION_PATH ?= $(abspath $(shell pwd)/../../../)
 
EXTRA_COMPONENT_DIRS += $(IOT_SOLUTION_PATH)/components
EXTRA_COMPONENT_DIRS += $(IOT_SOLUTION_PATH)/components/general/param
EXTRA_COMPONENT_DIRS += $(IOT_SOLUTION_PATH)/components/spi_devices
EXTRA_COMPONENT_DIRS += $(IOT_SOLUTION_PATH)/components/hmi/ugfx_gui
EXTRA_COMPONENT_DIRS += $(IOT_SOLUTION_PATH)/components/hmi/gdrivers
EXTRA_COMPONENT_DIRS += $(IOT_SOLUTION_PATH)/components/i2s_devices
EXTRA_COMPONENT_DIRS += $(IOT_SOLUTION_PATH)/components/i2c_devices/i2c_bus
EXTRA_COMPONENT_DIRS += $(IOT_SOLUTION_PATH)/components/i2c_devices/sensor/ft5x06

include $(IDF_PATH)/make/project.mk