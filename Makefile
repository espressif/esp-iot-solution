#
# This is a project Makefile. It is assumed the directory this Makefile resides in is a
# project subdirectory.
#

PROJECT_NAME := iot
EXTRA_COMPONENT_DIRS := $(shell pwd)/products $(shell pwd)/abstract $(shell pwd)/platforms

include $(IDF_PATH)/make/project.mk

