
file(GLOB_RECURSE srcs lvgl/src/*.c)

list(APPEND srcs "lvgl_gui.c"
                "lvgl_adapter.c")

set(incdirs "."
            "lvgl"
            "lvgl/lv_core"
            "lvgl/lv_draw"
            "lvgl/lv_fonts"
            "lvgl/lv_hal"
            "lvgl/lv_misc"
            "lvgl/lv_objx"
            "lvgl/lv_themes")
            
    set(srcdirs "."
            "lvgl/lv_core"
            "lvgl/lv_draw"
            "lvgl/lv_fonts"
            "lvgl/lv_hal"
            "lvgl/lv_misc"
            "lvgl/lv_objx"
            "lvgl/lv_themes")
endif()

idf_component_register(SRC_DIRS "${srcdirs}"
                        INCLUDE_DIRS "${incdirs}"
                        REQUIRES param gdisp touch_panel)

target_compile_definitions(${COMPONENT_LIB} INTERFACE LV_CONF_INCLUDE_SIMPLE=1)
