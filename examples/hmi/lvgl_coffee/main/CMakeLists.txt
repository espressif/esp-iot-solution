idf_component_register(SRCS "app_main.cpp"
                            "coffee_bean.c"
                            "coffee_cup.c"
                            "coffee_flower.c"
                        INCLUDE_DIRS ".")

target_compile_options(${COMPONENT_LIB} PRIVATE -Wno-cast-function-type)