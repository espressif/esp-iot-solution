if(CONFIG_ELF_SHELL)
    set(srcs  "src/shell_init.c")
    set(include_dir "include")
    set(priv_include_dir "private_include")

    if(CONFIG_SHELL_CMD_LS)
        list(APPEND srcs "src/shell_ls.c")
    endif()

    if(CONFIG_SHELL_CMD_EXEC)
        list(APPEND srcs "src/shell_exec.c")
    endif()

    if(CONFIG_SHELL_CMD_INSMOD)
        list(APPEND srcs "src/shell_insmod.c")
    endif()

    if(CONFIG_SHELL_CMD_LIST)
        list(APPEND srcs "src/shell_list.c")
    endif()

    if(CONFIG_SHELL_CMD_RMMOD)
        list(APPEND srcs "src/shell_rmmod.c")
    endif()

    if(CONFIG_SHELL_CMD_FREE)
        list(APPEND srcs "src/shell_free.c")
    endif()
endif()

idf_component_register(SRCS ${srcs}
                       INCLUDE_DIRS ${include_dir}
                       PRIV_INCLUDE_DIRS ${priv_include_dir}
                       REQUIRES "console" "elf_loader")
