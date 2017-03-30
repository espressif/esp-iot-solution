#
# Component Makefile
#
# LIBS := joylink
COMPONENT_ADD_INCLUDEDIRS := . extern joylink json list lua example smnt jdinnet
COMPONENT_SRCDIRS := app auth extern joylink json list lua smnt jdinnet upgrade

CFLAGS += -DPLATFORM_ESP32
