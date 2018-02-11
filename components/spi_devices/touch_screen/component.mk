#
# Main Makefile. This is basically the same as a component makefile.
#

ifdef CONFIG_IOT_TOUCH_SCREEN_ENABLE
COMPONENT_ADD_INCLUDEDIRS := include
COMPONENT_SRCDIRS := .
else
# Disable component
COMPONENT_ADD_INCLUDEDIRS :=
COMPONENT_ADD_LDFLAGS :=
COMPONENT_SRCDIRS :=
endif