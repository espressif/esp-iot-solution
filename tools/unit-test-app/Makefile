#
# This is a project Makefile. It is assumed the directory this Makefile resides in is a
# project subdirectory.
#

PROJECT_NAME := unit-test-app
#If IOT_SOLUTION_PATH is not defined, use relative path as default value
IOT_SOLUTION_PATH ?= $(abspath $(shell pwd)/../../)

IOT_COMPONENT_DIRS += $(IOT_SOLUTION_PATH)/components
IOT_COMPONENT_DIRS += $(IOT_SOLUTION_PATH)/components/features
IOT_COMPONENT_DIRS += $(IOT_SOLUTION_PATH)/components/general
IOT_COMPONENT_DIRS += $(IOT_SOLUTION_PATH)/components/wifi
IOT_COMPONENT_DIRS += $(IOT_SOLUTION_PATH)/components/spi_devices
IOT_COMPONENT_DIRS += $(IOT_SOLUTION_PATH)/components/i2c_devices
IOT_COMPONENT_DIRS += $(IOT_SOLUTION_PATH)/components/i2c_devices/sensor
IOT_COMPONENT_DIRS += $(IOT_SOLUTION_PATH)/components/i2c_devices/others
IOT_COMPONENT_DIRS += $(IOT_SOLUTION_PATH)/components/network
IOT_COMPONENT_DIRS += $(IOT_SOLUTION_PATH)/components/platforms
IOT_COMPONENT_DIRS += $(IOT_SOLUTION_PATH)/components/motor/stepper
IOT_COMPONENT_DIRS += $(IOT_SOLUTION_PATH)/components/motor/servo
IOT_COMPONENT_DIRS += $(IOT_SOLUTION_PATH)/components/framework
IOT_COMPONENT_DIRS += $(IOT_SOLUTION_PATH)/components/i2s_devices
# IOT_COMPONENT_DIRS += $(IOT_SOLUTION_PATH)/components/hmi

EXTRA_COMPONENT_DIRS += $(IOT_COMPONENT_DIRS)

#Do not touch the code below
IOT_COMPONENT_DIRS += $(abspath $(shell pwd)/components)
# Find all component names. The component names are the same as the
# directories they're in, so /bla/components/mycomponent/component.mk -> mycomponent.
IOT_COMPONENTS := $(dir $(foreach cd,$(IOT_COMPONENT_DIRS),                           \
					$(wildcard $(cd)/*/component.mk) $(wildcard $(cd)/component.mk) \
				))
IOT_COMPONENTS := $(sort $(foreach comp,$(IOT_COMPONENTS),$(lastword $(subst /, ,$(comp)))))

# If IOT_ALL set to 1, set TEST_COMPONENTS to all IoT components
ifeq ($(IOT_TEST_ALL),1)
TEST_COMPONENTS := $(IOT_COMPONENTS)
else
# otherwise, use TEST_COMPONENTS
#TEST_COMPONENTS_LIST := $(TEST_COMPONENTS)
endif

include $(IDF_PATH)/make/project.mk

list-components:
	echo TEST_COMPONENTS=$(TEST_COMPONENTS) 
	echo IOT_COMPONENTS=$(IOT_COMPONENTS) 
	echo TEST_COMPONENTS_LIST=$(TEST_COMPONENTS_LIST)

print_flash_cmd:
	echo $(ESPTOOL_WRITE_FLASH_OPTIONS) $(ESPTOOL_ALL_FLASH_ARGS) | sed -e 's:'$(PWD)/build/'::g'
