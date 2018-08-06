#
# "main" pseudo-component makefile.
#
# (Uses default behaviour of compiling all source files in directory, adding 'include' to include path.)

# componet standalone mode
ifndef CONFIG_IOT_SOLUTION_EMBED   


COMPONENT_ADD_INCLUDEDIRS := include

ifdef CONFIG_DATA_SCOPE_DEBUG
COMPONENT_ADD_INCLUDEDIRS += scope_debug	
COMPONENT_SRCDIRS := . scope_debug
endif

else

ifdef CONFIG_IOT_TOUCH_ENABLE
COMPONENT_ADD_INCLUDEDIRS := include

ifdef CONFIG_DATA_SCOPE_DEBUG
COMPONENT_ADD_INCLUDEDIRS += scope_debug	
COMPONENT_SRCDIRS := . scope_debug
endif

else
# Disable component
COMPONENT_ADD_INCLUDEDIRS :=
COMPONENT_ADD_LDFLAGS :=
COMPONENT_SRCDIRS :=
endif

endif