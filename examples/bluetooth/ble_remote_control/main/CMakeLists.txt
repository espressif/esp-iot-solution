idf_component_register(SRCS "controller.c" "hidd.c"
                            "gap_gatts.c"
                            "main.c"
                    INCLUDE_DIRS ".")

target_compile_options(${COMPONENT_LIB} PRIVATE -Wno-unused-const-variable)
