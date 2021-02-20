
file(GLOB_RECURSE SOURCES lv_examples/*.c)

idf_component_register(SRCS ${SOURCES}
                       INCLUDE_DIRS .
                       REQUIRES lvgl_gui)

target_compile_definitions(${COMPONENT_LIB} INTERFACE LV_LVGL_H_INCLUDE_SIMPLE=1)
target_compile_options(${COMPONENT_LIB} PUBLIC -Wno-cast-function-type -Wno-unused-variable -Wno-unused-function)
