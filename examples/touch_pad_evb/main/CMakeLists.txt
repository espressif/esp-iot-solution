
set(srcs "evb_adc.c"
            "evb_rgb_led.c"
            "evb_seg_led.c"
            "evb_touch_button.cpp"
            "evb_touch_matrix.cpp"
            "evb_touch_seq_slide.cpp"
            "evb_touch_slide.cpp"
            "evb_touch_spring.cpp"
            "evb_touch_wheel.cpp"
            "main.cpp")

idf_component_register(SRCS "${srcs}"
                        INCLUDE_DIRS "."
                        REQUIRES )