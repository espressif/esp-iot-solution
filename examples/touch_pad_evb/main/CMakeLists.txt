
set(COMPONENT_SRCS "evb_adc.c"
                    "evb_rgb_led.cpp"
                    "evb_seg_led.c"
                    "evb_touch_button.cpp"
                    "evb_touch_matrix.cpp"
                    "evb_touch_seq_slide.cpp"
                    "evb_touch_slide.cpp"
                    "evb_touch_spring.cpp"
                    "evb_touch_wheel.cpp"
                    "main.cpp")

set(COMPONENT_ADD_INCLUDEDIRS ".")

set(COMPONENT_REQUIRES touchpad led esp_adc_cal ch450 light)

register_component()
