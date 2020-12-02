#
# "main" pseudo-component makefile.
#
# (Uses default behaviour of compiling all source files in directory, adding 'include' to include path.)


SCREEN_DIR = screen_color/ili9341 \
            screen_color/st7789 \
            screen_color/st7796 \
            screen_color/nt35510 \
            screen_color/ili9806 \
            screen_color/ili9486 \
            screen_color/ssd1351 \
            screen_mono/ssd1306 \
            screen_mono/ssd1307 \

COMPONENT_ADD_INCLUDEDIRS := . iface_driver $(SCREEN_DIR)
COMPONENT_SRCDIRS := . iface_driver $(SCREEN_DIR)
