#
# "main" pseudo-component makefile.
#
# (Uses default behaviour of compiling all source files in directory, adding 'include' to include path.)

ifndef CONFIG_UGFX_GUI_ENABLE
COMPONENT_ADD_INCLUDEDIRS := 
COMPONENT_SRCDIRS :=
else
UGFXLIB = ugfx

COMPONENT_ADD_INCLUDEDIRS := .\
    ./include \
    $(UGFXLIB) \
    $(UGFXLIB)/src \

# COMPONENT_PRIV_INCLUDEDIRS +=  .\
#     ./include \
#     $(UGFXLIB) \
#     $(UGFXLIB)/src \

ifdef CONFIG_UGFX_USE_CUSTOM_DRIVER
COMPONENT_DEPENDS += $(call dequote,$(CONFIG_UGFX_CUSTOM_DRIVER_COMPONENT_NAME))
else
COMPONENT_DEPENDS += gdrivers
endif

COMPONENT_EXTRA_INCLUDES += $(PROJECT_PATH)/$(call dequote,$(CONFIG_UGFX_PROJ_RESOURCE_PATH))  

# In order to reduce the warning, turn off all warnings of UGFX
CFLAGS += -w
CXXFLAGS += -w
endif  #CONFIG_UGFX_GUI_ENABLE
