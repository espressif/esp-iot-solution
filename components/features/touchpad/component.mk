#
# "main" pseudo-component makefile.
#
# (Uses default behaviour of compiling all source files in directory, adding 'include' to include path.)


COMPONENT_ADD_INCLUDEDIRS := include
COMPONENT_SRCDIRS := .

ifdef CONFIG_DATA_SCOPE_DEBUG
COMPONENT_ADD_INCLUDEDIRS += scope_debug	
COMPONENT_SRCDIRS += scope_debug
endif