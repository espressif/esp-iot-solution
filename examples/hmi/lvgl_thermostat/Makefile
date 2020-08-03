#
# This is a project Makefile. It is assumed the directory this Makefile resides in is a
# project subdirectory.
#

PROJECT_NAME := lvgl_thermostat
#If IOT_SOLUTION_PATH is not defined, use relative path as default value
IOT_SOLUTION_PATH ?= $(abspath $(shell pwd)/../../)
 
EXTRA_COMPONENT_DIRS += $(IOT_SOLUTION_PATH)/components
EXTRA_COMPONENT_DIRS += $(IOT_SOLUTION_PATH)/components/general/param
EXTRA_COMPONENT_DIRS += $(IOT_SOLUTION_PATH)/components/spi_devices
EXTRA_COMPONENT_DIRS += $(IOT_SOLUTION_PATH)/components/hmi/lvgl_gui
EXTRA_COMPONENT_DIRS += $(IOT_SOLUTION_PATH)/components/hmi/gdrivers

include $(IDF_PATH)/make/project.mk