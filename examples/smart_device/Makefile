#
# This is a project Makefile. It is assumed the directory this Makefile resides in is a
# project subdirectory.
#

PROJECT_NAME := smart_device
#If IOT_SOLUTION_PATH is not defined, use relative path as default value
IOT_SOLUTION_PATH ?= $(shell pwd)/../../
EXTRA_COMPONENT_DIRS := $(shell pwd)/abstract $(IOT_SOLUTION_PATH)/components $(IOT_SOLUTION_PATH)/products $(IOT_SOLUTION_PATH)/abstract $(IOT_SOLUTION_PATH)/platforms  

include $(IDF_PATH)/make/project.mk

