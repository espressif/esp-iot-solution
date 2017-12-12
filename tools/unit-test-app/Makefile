#
# This is a project Makefile. It is assumed the directory this Makefile resides in is a
# project subdirectory.
#

PROJECT_NAME := unit-test-app
#If IOT_SOLUTION_PATH is not defined, use relative path as default value
IOT_SOLUTION_PATH ?= $(abspath $(shell pwd)/../../)
include $(IOT_SOLUTION_PATH)/Makefile

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
print_flash_cmd:
	echo $(ESPTOOL_WRITE_FLASH_OPTIONS) $(ESPTOOL_ALL_FLASH_ARGS) | sed -e 's:'$(PWD)/build/'::g'
