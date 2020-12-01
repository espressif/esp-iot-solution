if(NOT CONFIG_LVGL_GUI_ENABLE)
    set(srcdirs "")
    set(incdirs "")
    message(STATUS "Building empty lvgl component due to configuration")
else()
    set(incdirs "."
            "include"
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
                        REQUIRES gdrivers param)

target_compile_options(${COMPONENT_LIB} INTERFACE -Wno-cast-function-type)
