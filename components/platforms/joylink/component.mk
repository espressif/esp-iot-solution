#
# Component Makefile
#
# LIBS := joylink
ifdef CONFIG_IOT_JOYLINK_ENABLE
COMPONENT_ADD_INCLUDEDIRS := . extern joylink json list lua example smnt jdinnet
COMPONENT_SRCDIRS := app auth extern joylink json list lua smnt jdinnet upgrade
CFLAGS += -DPLATFORM_ESP32
else
# Disable component
COMPONENT_ADD_INCLUDEDIRS :=
COMPONENT_ADD_LDFLAGS :=
COMPONENT_SRCDIRS :=
endif