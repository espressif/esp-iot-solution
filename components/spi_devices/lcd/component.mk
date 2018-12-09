#
# "main" pseudo-component makefile.
#
# (Uses default behaviour of compiling all source files in directory, adding 'include' to include path.)
CXXFLAGS += -D__AVR_ATtiny85__

# componet standalone mode
ifndef CONFIG_IOT_SOLUTION_EMBED   
COMPONENT_ADD_INCLUDEDIRS := include Adafruit-GFX-Library/Fonts Adafruit-GFX-Library
COMPONENT_SRCDIRS := . Adafruit-GFX-Library

else

ifdef CONFIG_IOT_LCD_ENABLE
COMPONENT_ADD_INCLUDEDIRS := include Adafruit-GFX-Library/Fonts Adafruit-GFX-Library
COMPONENT_SRCDIRS := . Adafruit-GFX-Library

else
# Disable component
COMPONENT_ADD_INCLUDEDIRS :=
COMPONENT_ADD_LDFLAGS :=
COMPONENT_SRCDIRS :=
endif

endif