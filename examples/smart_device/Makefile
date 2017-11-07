#
# This is a project Makefile. It is assumed the directory this Makefile resides in is a
# project subdirectory.
#

PROJECT_NAME := smart_device
#If IOT_SOLUTION_PATH is not defined, use relative path as default value
IOT_SOLUTION_PATH ?= $(shell pwd)/../../
 
EXTRA_COMPONENT_DIRS := $(shell pwd)/abstract
EXTRA_COMPONENT_DIRS += $(shell pwd)/products
include $(IOT_SOLUTION_PATH)/Makefile
