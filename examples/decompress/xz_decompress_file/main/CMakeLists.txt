idf_component_register(SRCS "xz_decompress_file_example_main.c"
                    INCLUDE_DIRS "."
                    REQUIRES bootloader_custom_malloc esp-xz
                    EMBED_TXTFILES ${project_dir}/test_file/hello.txt ${project_dir}/test_file/hello.txt.xz)
