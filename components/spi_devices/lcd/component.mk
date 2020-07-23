#
# "main" pseudo-component makefile.
#
# (Uses default behaviour of compiling all source files in directory, adding 'include' to include path.)
CXXFLAGS += -D__AVR_ATtiny85__

COMPONENT_ADD_INCLUDEDIRS := include Adafruit-GFX-Library/Fonts Adafruit-GFX-Library
COMPONENT_SRCDIRS := . Adafruit-GFX-Library