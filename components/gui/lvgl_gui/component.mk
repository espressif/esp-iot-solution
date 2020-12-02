#
# Component Makefile
#

# Set simple includes as default
ifndef LV_CONF_INCLUDE_SIMPLE
CFLAGS += -DLV_CONF_INCLUDE_SIMPLE
endif

endif  #CONFIG_LVGL_GUI_ENABLE