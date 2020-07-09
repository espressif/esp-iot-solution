#
# Component Makefile
#

COMPONENT_ADD_INCLUDEDIRS := esp32-alink/include esp32-alink/adaptation/include
COMPONENT_SRCDIRS := adaptation esp32-alink/application
ifdef CONFIG_ALINK_VERSION_EMBED
LIBS := alink_agent tfspal
endif
ifdef CONFIG_ALINK_VERSION_SDS
LIBS := alink_agent_sds tfspal
endif
COMPONENT_ADD_LDFLAGS += -L $(COMPONENT_PATH)/esp32-alink/lib \
                           $(addprefix -l,$(LIBS))

