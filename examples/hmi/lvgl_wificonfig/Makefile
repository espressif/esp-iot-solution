#
# This is a project Makefile. It is assumed the directory this Makefile resides in is a
# project subdirectory.
#

PROJECT_NAME := lvgl_wificonfig

#If IOT_SOLUTION_PATH is not defined, use relative path as default value
IOT_SOLUTION_PATH ?= $(abspath $(shell pwd)/../../../)
 
EXTRA_COMPONENT_DIRS += $(IOT_SOLUTION_PATH)/components
EXTRA_COMPONENT_DIRS += $(IOT_SOLUTION_PATH)/components/general/param
EXTRA_COMPONENT_DIRS += $(IOT_SOLUTION_PATH)/components/bus
EXTRA_COMPONENT_DIRS += $(IOT_SOLUTION_PATH)/components/gui/ginput/touch
EXTRA_COMPONENT_DIRS += $(IOT_SOLUTION_PATH)/components/gui/lvgl_gui
EXTRA_COMPONENT_DIRS += $(IOT_SOLUTION_PATH)/components/gui/gdisp
EXTRA_COMPONENT_DIRS += $(IOT_SOLUTION_PATH)/components/gui/lcd_paint

include $(IDF_PATH)/make/project.mk