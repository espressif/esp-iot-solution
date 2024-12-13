if(CONFIG_IDF_TARGET_ARCH_XTENSA)
    set(TEST_ELF "test_xtensa.elf")
elseif(CONFIG_IDF_TARGET_ARCH_RISCV)
    set(TEST_ELF "test_riscv.elf")
endif()

idf_component_register(SRCS "elf_loader_example_main.c"
                       INCLUDE_DIRS ""
                       EMBED_TXTFILES ${TEST_ELF})
