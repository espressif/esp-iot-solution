#
# "main" pseudo-component makefile.
#
# (Uses default behaviour of compiling all source files in directory, adding 'include' to include path.)

# componet standalone mode
ifndef CONFIG_IOT_SOLUTION_EMBED   

COMPONENT_ADD_INCLUDEDIRS := ./power_meter/include
COMPONENT_SRCDIRS := ./power_meter

else

ifdef CONFIG_IOT_POWER_METER_ENABLE
COMPONENT_ADD_INCLUDEDIRS := ./power_meter/include
COMPONENT_SRCDIRS := ./power_meter
else
# Disable component
COMPONENT_ADD_INCLUDEDIRS :=
COMPONENT_ADD_LDFLAGS :=
COMPONENT_SRCDIRS :=
endif

endif