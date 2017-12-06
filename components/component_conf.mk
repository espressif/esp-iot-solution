#
# "main" pseudo-component makefile.
#
# (Uses default behaviour of compiling all source files in directory, adding 'include' to include path.)

EXTRA_COMPONENT_DIRS += $(IOT_SOLUTION_PATH)/components/__config
EXTRA_COMPONENT_DIRS += $(IOT_SOLUTION_PATH)/components/features
EXTRA_COMPONENT_DIRS += $(IOT_SOLUTION_PATH)/components/general
EXTRA_COMPONENT_DIRS += $(IOT_SOLUTION_PATH)/components/wifi
EXTRA_COMPONENT_DIRS += $(IOT_SOLUTION_PATH)/components/spi_devices
EXTRA_COMPONENT_DIRS += $(IOT_SOLUTION_PATH)/components/i2c_devices
EXTRA_COMPONENT_DIRS += $(IOT_SOLUTION_PATH)/components/i2c_devices/sensor
EXTRA_COMPONENT_DIRS += $(IOT_SOLUTION_PATH)/components/i2c_devices/others
EXTRA_COMPONENT_DIRS += $(IOT_SOLUTION_PATH)/components/network
EXTRA_COMPONENT_DIRS += $(IOT_SOLUTION_PATH)/components/platforms
