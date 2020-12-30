#
# "main" pseudo-component makefile.
#
# (Uses default behaviour of compiling all source files in directory, adding 'include' to include path.)

TOUCH_DIR = xpt2046 ft5x06 ns2016

COMPONENT_ADD_INCLUDEDIRS := . $(TOUCH_DIR)
COMPONENT_PRIV_INCLUDEDIRS := calibration
COMPONENT_SRCDIRS := . $(TOUCH_DIR) calibration
