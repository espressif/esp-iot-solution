#
# Component Makefile
#

ifdef CONFIG_BOARD_ESP32_DEVKITC_V4
    COMPONENT_SRCDIRS := esp32-devkitc-v4
else ifdef CONFIG_BOARD_ESP32S2_SAOLA_1
    COMPONENT_SRCDIRS := esp32s2-saola-1
else ifdef CONFIG_BOARD_ESP32_MESHKIT_SENSE
    COMPONENT_SRCDIRS := esp32-meshkit-sense
else ifdef CONFIG_BOARD_ESP32_LCDKIT
    COMPONENT_SRCDIRS := esp32-lcdkit
endif

COMPONENT_ADD_INCLUDEDIRS := $(COMPONENT_SRCDIRS)

$(COMPONENT_SRCDIRS)/board.o:
	@echo "-----------Board Info---------"
	@echo "IDF_TARGET = $(IDF_TARGET)"
	@echo "Board DIR = $(COMPONENT_SRCDIRS)"
	@echo "---------Board Info End---------"

