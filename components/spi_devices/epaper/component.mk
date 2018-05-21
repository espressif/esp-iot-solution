#
# Main Makefile. This is basically the same as a component makefile.
#

# componet standalone mode
ifndef CONFIG_IOT_SOLUTION_EMBED   

COMPONENT_ADD_INCLUDEDIRS := include
COMPONENT_SRCDIRS := .

else

ifdef CONFIG_IOT_EINK_ENABLE
COMPONENT_ADD_INCLUDEDIRS := include
COMPONENT_SRCDIRS := .
else
# Disable component
COMPONENT_ADD_INCLUDEDIRS :=
COMPONENT_ADD_LDFLAGS :=
COMPONENT_SRCDIRS :=
endif

endif