#
# This is a project Makefile. It is assumed the directory this Makefile resides in is a
# project subdirectory.
#

PROJECT_NAME := empty_project

#If IOT_SOLUTION_PATH is not defined, use relative path as default value
IOT_SOLUTION_PATH ?= $(abspath $(shell pwd)/../../)

EXTRA_COMPONENT_DIRS += $(IOT_SOLUTION_PATH)/components/general

include $(IDF_PATH)/make/project.mk