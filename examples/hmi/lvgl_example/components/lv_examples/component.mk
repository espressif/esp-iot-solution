#
# Component Makefile
#

CFLAGS += -DLV_LVGL_H_INCLUDE_SIMPLE

COMPONENT_SRCDIRS := lv_examples           \
    lv_examples/assets                     \
    lv_examples/src/lv_demo_benchmark      \
    lv_examples/src/lv_demo_keypad_encoder \
    lv_examples/src/lv_demo_printer        \
    lv_examples/src/lv_demo_printer/images \
    lv_examples/src/lv_demo_stress         \
    lv_examples/src/lv_demo_widgets        \
    lv_examples/src/lv_ex_style            \
    lv_examples/src/lv_ex_get_started      

COMPONENT_ADD_INCLUDEDIRS := $(COMPONENT_SRCDIRS) .
