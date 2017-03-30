#
# Component Makefile
#

# LIBS := joylink
COMPONENT_ADD_INCLUDEDIRS := . extern joylink json list lua example smnt jdinnet ble

LIBS := joylink_ble

COMPONENT_ADD_LDFLAGS += -L $(COMPONENT_PATH)/lib \
                         $(addprefix -l,$(LIBS)) \
                         $(LINKER_SCRIPTS)

COMPONENT_SRCDIRS := app auth extern joylink json list lua smnt jdinnet upgrade ble

CFLAGS += -DPLATFORM_ESP32

