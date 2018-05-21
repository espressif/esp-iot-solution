#
# "main" pseudo-component makefile.
#
# (Uses default behaviour of compiling all source files in directory, adding 'include' to include path.)

# componet standalone mode
ifndef CONFIG_IOT_SOLUTION_EMBED   

COMPONENT_ADD_INCLUDEDIRS := ./status_led/include
COMPONENT_SRCDIRS := ./status_led

else

ifdef CONFIG_IOT_LED_ENABLE
COMPONENT_ADD_INCLUDEDIRS := ./status_led/include
COMPONENT_SRCDIRS := ./status_led
else
# Disable component
COMPONENT_ADD_INCLUDEDIRS :=
COMPONENT_ADD_LDFLAGS :=
COMPONENT_SRCDIRS :=
endif

endif