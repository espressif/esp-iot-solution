set(LV_UI_DIR ui)
file(GLOB_RECURSE LV_UI_SOURCES ${LV_UI_DIR}/*.c)
set(CORE_DIR core)
file(GLOB_RECURSE CORE_SOURCES ${CORE_DIR}/*.c)

set(INCLUDE_DIRS "")
list(APPEND INCLUDE_DIRS "include")

idf_component_get_property(LVGL_LIB lvgl__lvgl COMPONENT_LIB)

idf_component_register(
    SRCS main.c app_lcd.c app_video.c camera_ppa_blend.c ${LV_UI_SOURCES} ${CORE_SOURCES}
    INCLUDE_DIRS . ${LV_UI_DIR} ${INCLUDE_DIRS} ${CORE_DIR}
)

target_compile_options(
    ${LVGL_LIB}
    PRIVATE
        -DLV_LVGL_H_INCLUDE_SIMPLE
        -DLV_USE_DEMO_MUSIC
        -Wno-missing-field-initializers
)
