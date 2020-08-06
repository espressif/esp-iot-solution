
if(CONFIG_UGFX_GUI_ENABLE)
    if(NOT CONFIG_UGFX_USE_CUSTOM_DRIVER)

        set(srcs "")
        set(incs "include")

        #Display driver mode
        if(CONFIG_UGFX_LCD_DRIVER_FRAMEBUFFER_MODE)
            set(srcs "${srcs}"
                                "gdisp/framebuffer/gdisp_lld_framebuffer.c"
                                )
            set(incs "${incs}"
                                            "gdisp/framebuffer")
        endif()

        #Display driver
        if(CONFIG_UGFX_DRIVER_ILI9341)
            set(srcs "${srcs}"
                                "gdisp/ILI9341/gdisp_lld_ILI9341.c"
                                "gdisp/ILI9341/ILI9341_adapter.cpp"
                                )
            set(incs "${incs}"
                                            "gdisp/ILI9341")
        endif()

        if(CONFIG_UGFX_DRIVER_ST7789)
            set(srcs "${srcs}"
                                "gdisp/ST7789/gdisp_lld_ST7789.c"
                                "gdisp/ST7789/ST7789_adapter.cpp"
                                )
            set(incs "${incs}"
                                            "gdisp/ST7789")
        endif()

        if(CONFIG_UGFX_DRIVER_SSD1306)
            set(srcs "${srcs}"
                                "gdisp/SSD1306/gdisp_lld_SSD1306.c"
                                "gdisp/SSD1306/SSD1306_adapter.cpp"
                                )
            set(incs "${incs}"
                                            "gdisp/SSD1306")
        endif()

        if(CONFIG_UGFX_DRIVER_NT35510)
            set(srcs "${srcs}"
                                "gdisp/NT35510/gdisp_lld_NT35510.c"
                                "gdisp/NT35510/NT35510_adapter.cpp"
                                )
            set(incs "${incs}"
                                            "gdisp/NT35510")
        endif()

        #Input driver
        #Touch driver
        if(CONFIG_UGFX_DRIVER_TOUCH_SCREEN_ENABLE)
            if(CONFIG_UGFX_DRIVER_TOUCH_XPT2046)
                set(srcs "${srcs}"
                                    "ginput/touch/XPT2046/gmouse_lld_XPT2046.c"
                                    "ginput/touch/XPT2046/XPT2046_adapter.cpp")
                set(incs "${incs}"
                                                "ginput/touch/XPT2046")
            endif()

            if(CONFIG_UGFX_DRIVER_TOUCH_FT5X06)
                set(srcs "${srcs}"
                                    "ginput/touch/FT5X06/gmouse_lld_FT5x06.c"
                                    "ginput/touch/FT5X06/lvgl_indev_adapter.c")
                set(incs "${incs}"
                                                "ginput/touch/FT5X06")
            endif()
        endif()

        #Toggle driver
        if(CONFIG_UGFX_DRIVER_TOGGLE_ENABLE)
            if(CONFIG_UGFX_DRIVER_TOGGLE_BUTTON)
                set(srcs "${srcs}"
                                    "ginput/toggle/button/ginput_lld_toggle_button.c")
                set(incs "${incs}"
                                                "ginput/toggle/button")
            endif()

            if(CONFIG_UGFX_DRIVER_TOGGLE_TOUCHPAD)
                set(srcs "${srcs}"
                                    "ginput/toggle/touchpad/ginput_lld_toggle_touchpad.c")
                set(incs "${incs}"
                                                "ginput/toggle/touchpad")
            endif()
        endif()
        
        #Audio driver
        if(CONFIG_UGFX_DRIVER_AUDIO_ENABLE)
            set(srcs "${srcs}"
                                "gaudio/dac_audio/gaudio_play_dac_audio.c"
                                )
            set(incs "${incs}"
                                            "gaudio/dac_audio")
        endif()

    endif()
else()
    if(CONFIG_LVGL_GUI_ENABLE)
        if(NOT CONFIG_LVGL_USE_CUSTOM_DRIVER)
            set(srcs "")

            set(incs "include")

            #Display driver mode
            if(CONFIG_LVGL_LCD_DRIVER_FRAMEBUFFER_MODE)
                set(srcs "${srcs}"
                                    "gdisp/framebuffer/gdisp_lld_framebuffer.c")
                set(incs "${incs}"
                                                "gdisp/framebuffer")
            endif()

            #Display driver
            if(CONFIG_LVGL_DRIVER_ILI9341)
                set(srcs "${srcs}"
                                    "gdisp/ILI9341/gdisp_lld_ILI9341.c"
                                    "gdisp/ILI9341/ILI9341_adapter.cpp")
                set(incs "${incs}"
                                                "gdisp/ILI9341")
            endif()

            if(CONFIG_LVGL_DRIVER_ST7789)
                set(srcs "${srcs}"
                                    "gdisp/ST7789/gdisp_lld_ST7789.c"
                                    "gdisp/ST7789/ST7789_adapter.cpp")
                set(incs "${incs}"
                                                "gdisp/ST7789")
            endif()

            if(CONFIG_LVGL_DRIVER_SSD1306)
                set(srcs "${srcs}"
                                    "gdisp/SSD1306/gdisp_lld_SSD1306.c"
                                    "gdisp/SSD1306/SSD1306_adapter.cpp")
                set(incs "${incs}"
                                                "gdisp/SSD1306")
            endif()

            if(CONFIG_LVGL_DRIVER_NT35510)
                set(srcs "${srcs}"
                                    "gdisp/NT35510/gdisp_lld_NT35510.c"
                                    "gdisp/NT35510/NT35510_adapter.cpp")
                set(incs "${incs}"
                                                "gdisp/NT35510")
            endif()

            #Input driver
            #Touch driver
            if(CONFIG_LVGL_DRIVER_TOUCH_SCREEN_ENABLE)
                if(CONFIG_LVGL_DRIVER_TOUCH_XPT2046)
                    set(srcs "${srcs}"
                                        "ginput/touch/XPT2046/gmouse_lld_XPT2046.c"
                                        "ginput/touch/XPT2046/XPT2046_adapter.cpp")
                    set(incs "${incs}"
                                                    "ginput/touch/XPT2046")
                endif()

                if(CONFIG_LVGL_DRIVER_TOUCH_FT5X06)
                    set(srcs "${srcs}"
                                        "ginput/touch/FT5X06/gmouse_lld_FT5x06.c"
                                        "ginput/touch/FT5X06/lvgl_indev_adapter.c")
                    set(incs "${incs}"
                                                    "ginput/touch/FT5X06")
                endif()
            endif()

            #Toggle driver
            if(CONFIG_LVGL_DRIVER_TOGGLE_ENABLE)
                if(CONFIG_LVGL_DRIVER_TOGGLE_BUTTON)
                    set(srcs "${srcs}"
                                        "ginput/toggle/button/ginput_lld_toggle_button.c")
                    set(incs "${incs}"
                                                    "ginput/toggle/button")
                endif()

                if(CONFIG_LVGL_DRIVER_TOGGLE_TOUCHPAD)
                    set(srcs "${srcs}"
                                        "ginput/toggle/touchpad/ginput_lld_toggle_touchpad.c")
                    set(incs "${incs}"
                                                    "ginput/toggle/touchpad")
                endif()
            endif()
            
            #Audio driver
            if(CONFIG_LVGL_DRIVER_AUDIO_ENABLE)
                set(srcs "${srcs}"
                                    "gaudio/dac_audio/gaudio_play_dac_audio.c")
                set(incs "${incs}"
                                                "gaudio/dac_audio")
            endif()

        endif()
    else()
        set(srcs "")
        set(incs "")
    endif()
endif()

idf_component_register(SRCS "${srcs}"
                        INCLUDE_DIRS "${incs}"
                        REQUIRES lvgl_gui ugfx_gui nt35510 lcd_common ssd1306 lcd xpt2046 ft5x06 touchpad)

target_compile_options(${COMPONENT_LIB} PRIVATE $<$<COMPILE_LANGUAGE:C>:-Wno-duplicate-decl-specifier> -Wno-misleading-indentation $<$<COMPILE_LANGUAGE:C>:-Wno-discarded-qualifiers>)