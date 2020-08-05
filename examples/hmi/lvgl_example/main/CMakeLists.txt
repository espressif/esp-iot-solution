idf_component_register(SRCS "app_main.cpp"
                            "lv_test_theme.c"
                        INCLUDE_DIRS ".")
                        
target_compile_options(${COMPONENT_LIB} PRIVATE -Wno-cast-function-type)