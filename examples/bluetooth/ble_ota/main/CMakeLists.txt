idf_component_register(SRCS "app_main.c")

target_compile_options(${COMPONENT_LIB} PRIVATE -Wno-unused-const-variable)