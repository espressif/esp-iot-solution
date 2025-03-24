set(elf_app_name "hello_world")
set(elf_app_path "../../../../build_elf_file_example")
set(elf_bin_name "test_elf")

idf_component_register(SRCS "elf_loader_example_main.c"
                       INCLUDE_DIRS "")

elf_embed_binary(${elf_app_name} ${elf_app_path} ${elf_bin_name})
