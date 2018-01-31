#
# ULP support additions to component makefile.
#
# 1. ULP_APP_NAME must be unique (if multiple components use ULP)
#    Default value, override if necessary:
ULP_APP_NAME ?= ulp_$(COMPONENT_NAME)
#
# 2. Specify all assembly source files here.
#    Files should be placed into a separate directory (in this case, ulp/),
#    which should not be added to COMPONENT_SRCDIRS.
ULP_S_SOURCES = $(addprefix $(COMPONENT_PATH)/ulp/, \
	i2c_dev.S \
	i2c.S \
	stack.S \
	)
#
# 3. List all the component object files which include automatically
#    generated ULP export file, $(ULP_APP_NAME).h:
ULP_EXP_DEP_OBJECTS := main.o
#
# 4. Include build rules for ULP program 
include $(IDF_PATH)/components/ulp/component_ulp_common.mk
#
# Link object files and generate map file
$(ULP_ELF): $(ULP_OBJECTS) $(ULP_LD_SCRIPT)
	$(summary) ULP_LD $(patsubst $(PWD)/%,%,$(CURDIR))/$@
	$(ULP_LD) -o $@ -A elf32-esp32ulp -Map=$(ULP_MAP) -T $(ULP_LD_SCRIPT) $(ULP_OBJECTS)
#
# End of ULP support additions to component makefile.
#
