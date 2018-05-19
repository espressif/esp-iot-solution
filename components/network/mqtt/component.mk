#
# Component Makefile
#
# This Makefile should, at the very least, just include $(SDK_PATH)/make/component.mk. By default,
# this will take the sources in this directory, compile them and link them into
# lib(subdirectory_name).a in the build directory. This behaviour is entirely configurable,
# please read the SDK documents if you need to do this.
#

# componet standalone mode
ifndef CONFIG_IOT_SOLUTION_EMBED   

COMPONENT_ADD_INCLUDEDIRS := include espmqtt/lib/include
COMPONENT_SRCDIRS := espmqtt espmqtt/lib

else

ifdef CONFIG_IOT_MQTT_ENABLE
COMPONENT_ADD_INCLUDEDIRS := include espmqtt/lib/include
COMPONENT_SRCDIRS := espmqtt espmqtt/lib
else
# Disable component
COMPONENT_ADD_INCLUDEDIRS :=
COMPONENT_ADD_LDFLAGS :=
COMPONENT_SRCDIRS :=
endif

endif