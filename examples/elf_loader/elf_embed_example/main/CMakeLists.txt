set(elf_app_name "elf_example")

# Set the address of the compiled project.
# If it is an external project, for example: <project_root>/esp-iot-solution/examples/elf_loader/build_elf_file_example
# Configure elf_app_path to /path/to/build_elf_file_example: "${CMAKE_CURRENT_LIST_DIR}/../../build_elf_file_example"

set(elf_app_path "${CMAKE_CURRENT_LIST_DIR}/elf_example_project")
set(elf_bin_name "test_elf")

idf_component_register(SRCS "elf_embed_example_main.c")

elf_embed_binary(${elf_app_name} ${elf_app_path} ${elf_bin_name})
