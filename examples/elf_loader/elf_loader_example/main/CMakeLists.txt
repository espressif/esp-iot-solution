if(CONFIG_IDF_TARGET_ARCH_XTENSA)
    set(TEST_ELF "test_xtensa.elf")
elseif(CONFIG_IDF_TARGET_ARCH_RISCV)
    set(TEST_ELF "test_riscv.elf")
endif()

idf_component_register(SRCS "elf_loader_example_main.c"
                       INCLUDE_DIRS "")

# Align embedded ELF for ESP-IDF releases that do not support ALIGN in target_add_binary_data().
elf_embed_aligned_binary(${COMPONENT_LIB} "${TEST_ELF}" TEXT)
