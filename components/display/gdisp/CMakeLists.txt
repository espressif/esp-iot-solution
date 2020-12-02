
set(SCREEN_DIR "screen_color/ili9341"
            "screen_color/st7789"
            "screen_color/st7796"
            "screen_color/nt35510"
            "screen_color/ili9806"
            "screen_color/ili9486"
            "screen_color/ssd1351"
            "screen_mono/ssd1306"
            "screen_mono/ssd1307"
            "screen_mono/ssd1322")

idf_component_register(SRC_DIRS "${SCREEN_DIR}" "iface_driver" "."
                        INCLUDE_DIRS "${SCREEN_DIR}" "iface_driver" "."
                        PRIV_REQUIRES bus boards
                        )
