#
# "main" pseudo-component makefile.
#
# (Uses default behaviour of compiling all source files in directory, adding 'include' to include path.)

ifndef CONFIG_LVGL_GUI_ENABLE
COMPONENT_ADD_INCLUDEDIRS := 
COMPONENT_SRCDIRS := 
else
LVGLLIB = lvgl
CFLAGS += -Wno-cast-function-type -Wno-error=narrowing
COMPONENT_SRCDIRS := . \
    ./include \
    $(LVGLLIB) \
    $(LVGLLIB)/lv_core \
    $(LVGLLIB)/lv_draw \
    $(LVGLLIB)/lv_hal \
    $(LVGLLIB)/lv_misc \
    $(LVGLLIB)/lv_fonts \
    $(LVGLLIB)/lv_objx \
    $(LVGLLIB)/lv_themes \

COMPONENT_ADD_INCLUDEDIRS := . \
    ./include \
    $(LVGLLIB) \
    $(LVGLLIB)/lv_core \
    $(LVGLLIB)/lv_draw \
    $(LVGLLIB)/lv_hal \
    $(LVGLLIB)/lv_misc \
    $(LVGLLIB)/lv_fonts \
    $(LVGLLIB)/lv_objx \
    $(LVGLLIB)/lv_themes \

ifdef CONFIG_LVGL_USE_CUSTOM_DRIVER
COMPONENT_DEPENDS += $(call dequote,$(CONFIG_LVGL_CUSTOM_DRIVER_COMPONENT_NAME))
else
COMPONENT_DEPENDS += gdrivers
endif

endif  #CONFIG_LVGL_GUI_ENABLE