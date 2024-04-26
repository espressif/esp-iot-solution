file(GLOB_RECURSE IMAGE_SOURCES images/*.c)

set(LV_DEMO_DIR ../managed_components/lvgl__lvgl/demos)
file(GLOB_RECURSE LV_DEMOS_SOURCES ${LV_DEMO_DIR}/*.c)

idf_component_register(SRC_DIRS
                        "."
                        "./images"
                       INCLUDE_DIRS
                       ".")

target_compile_options(${COMPONENT_LIB} PRIVATE -DLV_LVGL_H_INCLUDE_SIMPLE)
