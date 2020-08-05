idf_component_register(SRCS "app_main.cpp"
                            "img_day.c"
                            "img_night.c"
                            "img_snow.c"
                        INCLUDE_DIRS ".")

target_compile_options(${COMPONENT_LIB} PRIVATE -Wno-cast-function-type)