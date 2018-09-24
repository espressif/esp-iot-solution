#
# "main" pseudo-component makefile.
#
# (Uses default behaviour of compiling all source files in directory, adding 'include' to include path.)

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
IOT_COMPONENT_DIRS += $(IOT_SOLUTION_PATH)/components/hmi

EXTRA_COMPONENT_DIRS += $(IOT_COMPONENT_DIRS)
