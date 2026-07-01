idf_component_register(SRCS "esp32_h4_kbd_kit.c"
                    INCLUDE_DIRS "include"
                    REQUIRES lamp_array_matrix keyboard_button espressif__led_strip
                             lijunru-hub__keyboard_rgb_matrix)
