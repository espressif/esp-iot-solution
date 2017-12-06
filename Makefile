#
# This is a project Makefile. It is assumed the directory this Makefile resides in is a
# project subdirectory.
#

#Overwrite the IDF_PATH to the esp-idf path in submodule.
IDF_PATH := $(IOT_SOLUTION_PATH)/submodule/esp-idf/
include $(IOT_SOLUTION_PATH)/components/component_conf.mk
