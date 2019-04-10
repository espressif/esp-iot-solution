
if(CONFIG_UGFX_GUI_ENABLE)
    if(NOT CONFIG_UGFX_USE_CUSTOM_DRIVER)

        set(COMPONENT_SRCS "")
        set(COMPONENT_ADD_INCLUDEDIRS "include")

        #Display driver mode
        if(CONFIG_UGFX_LCD_DRIVER_FRAMEBUFFER_MODE)
            set(COMPONENT_SRCS "${COMPONENT_SRCS}"
                                "gdisp/framebuffer/gdisp_lld_framebuffer.c"
                                )
            set(COMPONENT_ADD_INCLUDEDIRS "${COMPONENT_ADD_INCLUDEDIRS}"
                                            "gdisp/framebuffer")
        endif()

        #Display driver
        if(CONFIG_UGFX_DRIVER_ILI9341)
            set(COMPONENT_SRCS "${COMPONENT_SRCS}"
                                "gdisp/ILI9341/gdisp_lld_ILI9341.c"
                                "gdisp/ILI9341/ILI9341_adapter.cpp"
                                )
            set(COMPONENT_ADD_INCLUDEDIRS "${COMPONENT_ADD_INCLUDEDIRS}"
                                            "gdisp/ILI9341")
        endif()

        if(CONFIG_UGFX_DRIVER_ST7789)
            set(COMPONENT_SRCS "${COMPONENT_SRCS}"
                                "gdisp/ST7789/gdisp_lld_ST7789.c"
                                "gdisp/ST7789/ST7789_adapter.cpp"
                                )
            set(COMPONENT_ADD_INCLUDEDIRS "${COMPONENT_ADD_INCLUDEDIRS}"
                                            "gdisp/ST7789")
        endif()

        if(CONFIG_UGFX_DRIVER_SSD1306)
            set(COMPONENT_SRCS "${COMPONENT_SRCS}"
                                "gdisp/SSD1306/gdisp_lld_SSD1306.c"
                                "gdisp/SSD1306/SSD1306_adapter.cpp"
                                )
            set(COMPONENT_ADD_INCLUDEDIRS "${COMPONENT_ADD_INCLUDEDIRS}"
                                            "gdisp/SSD1306")
        endif()

        if(CONFIG_UGFX_DRIVER_NT35510)
            set(COMPONENT_SRCS "${COMPONENT_SRCS}"
                                "gdisp/NT35510/gdisp_lld_NT35510.c"
                                "gdisp/NT35510/NT35510_adapter.cpp"
                                )
            set(COMPONENT_ADD_INCLUDEDIRS "${COMPONENT_ADD_INCLUDEDIRS}"
                                            "gdisp/NT35510")
        endif()

        #Input driver
        #Touch driver
        if(CONFIG_UGFX_DRIVER_TOUCH_SCREEN_ENABLE)
            if(CONFIG_UGFX_DRIVER_TOUCH_XPT2046)
                set(COMPONENT_SRCS "${COMPONENT_SRCS}"
                                    "ginput/touch/XPT2046/gmouse_lld_XPT2046.c"
                                    "ginput/touch/XPT2046/XPT2046_adapter.cpp")
                set(COMPONENT_ADD_INCLUDEDIRS "${COMPONENT_ADD_INCLUDEDIRS}"
                                                "ginput/touch/XPT2046")
            endif()

            if(CONFIG_UGFX_DRIVER_TOUCH_FT5X06)
                set(COMPONENT_SRCS "${COMPONENT_SRCS}"
                                    "ginput/touch/FT5X06/gmouse_lld_FT5x06.c"
                                    "ginput/touch/FT5X06/lvgl_indev_adapter.c")
                set(COMPONENT_ADD_INCLUDEDIRS "${COMPONENT_ADD_INCLUDEDIRS}"
                                                "ginput/touch/FT5X06")
            endif()
        endif()

        #Toggle driver
        if(CONFIG_UGFX_DRIVER_TOGGLE_ENABLE)
            if(CONFIG_UGFX_DRIVER_TOGGLE_BUTTON)
                set(COMPONENT_SRCS "${COMPONENT_SRCS}"
                                    "ginput/toggle/button/ginput_lld_toggle_button.c")
                set(COMPONENT_ADD_INCLUDEDIRS "${COMPONENT_ADD_INCLUDEDIRS}"
                                                "ginput/toggle/button")
            endif()

            if(CONFIG_UGFX_DRIVER_TOGGLE_TOUCHPAD)
                set(COMPONENT_SRCS "${COMPONENT_SRCS}"
                                    "ginput/toggle/touchpad/ginput_lld_toggle_touchpad.c")
                set(COMPONENT_ADD_INCLUDEDIRS "${COMPONENT_ADD_INCLUDEDIRS}"
                                                "ginput/toggle/touchpad")
            endif()
        endif()
        
        #Audio driver
        if(CONFIG_UGFX_DRIVER_AUDIO_ENABLE)
            set(COMPONENT_SRCS "${COMPONENT_SRCS}"
                                "gaudio/dac_audio/gaudio_play_dac_audio.c"
                                )
            set(COMPONENT_ADD_INCLUDEDIRS "${COMPONENT_ADD_INCLUDEDIRS}"
                                            "gaudio/dac_audio")
        endif()

    endif()
else()
    if(CONFIG_LVGL_GUI_ENABLE)
        if(NOT CONFIG_LVGL_USE_CUSTOM_DRIVER)
            set(COMPONENT_SRCS "")

            set(COMPONENT_ADD_INCLUDEDIRS "include")

            #Display driver mode
            if(CONFIG_LVGL_LCD_DRIVER_FRAMEBUFFER_MODE)
                set(COMPONENT_SRCS "${COMPONENT_SRCS}"
                                    "gdisp/framebuffer/gdisp_lld_framebuffer.c")
                set(COMPONENT_ADD_INCLUDEDIRS "${COMPONENT_ADD_INCLUDEDIRS}"
                                                "gdisp/framebuffer")
            endif()

            #Display driver
            if(CONFIG_LVGL_DRIVER_ILI9341)
                set(COMPONENT_SRCS "${COMPONENT_SRCS}"
                                    "gdisp/ILI9341/gdisp_lld_ILI9341.c"
                                    "gdisp/ILI9341/ILI9341_adapter.cpp")
                set(COMPONENT_ADD_INCLUDEDIRS "${COMPONENT_ADD_INCLUDEDIRS}"
                                                "gdisp/ILI9341")
            endif()

            if(CONFIG_LVGL_DRIVER_ST7789)
                set(COMPONENT_SRCS "${COMPONENT_SRCS}"
                                    "gdisp/ST7789/gdisp_lld_ST7789.c"
                                    "gdisp/ST7789/ST7789_adapter.cpp")
                set(COMPONENT_ADD_INCLUDEDIRS "${COMPONENT_ADD_INCLUDEDIRS}"
                                                "gdisp/ST7789")
            endif()

            if(CONFIG_LVGL_DRIVER_SSD1306)
                set(COMPONENT_SRCS "${COMPONENT_SRCS}"
                                    "gdisp/SSD1306/gdisp_lld_SSD1306.c"
                                    "gdisp/SSD1306/SSD1306_adapter.cpp")
                set(COMPONENT_ADD_INCLUDEDIRS "${COMPONENT_ADD_INCLUDEDIRS}"
                                                "gdisp/SSD1306")
            endif()

            if(CONFIG_LVGL_DRIVER_NT35510)
                set(COMPONENT_SRCS "${COMPONENT_SRCS}"
                                    "gdisp/NT35510/gdisp_lld_NT35510.c"
                                    "gdisp/NT35510/NT35510_adapter.cpp")
                set(COMPONENT_ADD_INCLUDEDIRS "${COMPONENT_ADD_INCLUDEDIRS}"
                                                "gdisp/NT35510")
            endif()

            #Input driver
            #Touch driver
            if(CONFIG_LVGL_DRIVER_TOUCH_SCREEN_ENABLE)
                if(CONFIG_LVGL_DRIVER_TOUCH_XPT2046)
                    set(COMPONENT_SRCS "${COMPONENT_SRCS}"
                                        "ginput/touch/XPT2046/gmouse_lld_XPT2046.c"
                                        "ginput/touch/XPT2046/XPT2046_adapter.cpp")
                    set(COMPONENT_ADD_INCLUDEDIRS "${COMPONENT_ADD_INCLUDEDIRS}"
                                                    "ginput/touch/XPT2046")
                endif()

                if(CONFIG_LVGL_DRIVER_TOUCH_FT5X06)
                    set(COMPONENT_SRCS "${COMPONENT_SRCS}"
                                        "ginput/touch/FT5X06/gmouse_lld_FT5x06.c"
                                        "ginput/touch/FT5X06/lvgl_indev_adapter.c")
                    set(COMPONENT_ADD_INCLUDEDIRS "${COMPONENT_ADD_INCLUDEDIRS}"
                                                    "ginput/touch/FT5X06")
                endif()
            endif()

            #Toggle driver
            if(CONFIG_LVGL_DRIVER_TOGGLE_ENABLE)
                if(CONFIG_LVGL_DRIVER_TOGGLE_BUTTON)
                    set(COMPONENT_SRCS "${COMPONENT_SRCS}"
                                        "ginput/toggle/button/ginput_lld_toggle_button.c")
                    set(COMPONENT_ADD_INCLUDEDIRS "${COMPONENT_ADD_INCLUDEDIRS}"
                                                    "ginput/toggle/button")
                endif()

                if(CONFIG_LVGL_DRIVER_TOGGLE_TOUCHPAD)
                    set(COMPONENT_SRCS "${COMPONENT_SRCS}"
                                        "ginput/toggle/touchpad/ginput_lld_toggle_touchpad.c")
                    set(COMPONENT_ADD_INCLUDEDIRS "${COMPONENT_ADD_INCLUDEDIRS}"
                                                    "ginput/toggle/touchpad")
                endif()
            endif()
            
            #Audio driver
            if(CONFIG_LVGL_DRIVER_AUDIO_ENABLE)
                set(COMPONENT_SRCS "${COMPONENT_SRCS}"
                                    "gaudio/dac_audio/gaudio_play_dac_audio.c")
                set(COMPONENT_ADD_INCLUDEDIRS "${COMPONENT_ADD_INCLUDEDIRS}"
                                                "gaudio/dac_audio")
            endif()

        endif()
    else()
        set(COMPONENT_SRCS "")
        set(COMPONENT_ADD_INCLUDEDIRS "")
    endif()
endif()

# The values of COMPONENT_REQUIRES and COMPONENT_PRIV_REQUIRES should not 
# depend on any configuration choices (CONFIG_xxx macros). 
# This is because requirements are expanded before configuration is loaded. 
# Other component variables (like include paths or source files) can depend on configuration choices.
set(COMPONENT_REQUIRES lvgl_gui ugfx_gui nt35510 ssd1306 lcd xpt2046 ft5x06 touchpad)

register_component()
