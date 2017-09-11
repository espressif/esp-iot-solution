#
# This is a project Makefile. It is assumed the directory this Makefile resides in is a
# project subdirectory.
#

#Overwrite the IDF_PATH to the esp-idf path in submodule.
IDF_PATH := $(IOT_SOLUTION_PATH)/submodule/esp-idf/
EXTRA_COMPONENT_DIRS += $(IOT_SOLUTION_PATH)/components $(IOT_SOLUTION_PATH)/products $(IOT_SOLUTION_PATH)/platforms

include $(IDF_PATH)/make/project.mk

