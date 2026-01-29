set(srcs "")
set(include_dir "")
set(priv_include_dir "")

if(CONFIG_ELF_SHELL)
    list(APPEND srcs  "src/shell_init.c")
    list(APPEND include_dir  "include")
    list(APPEND priv_include_dir "private_include")

    if(CONFIG_SHELL_CMD_LS)
        list(APPEND srcs "src/shell_ls.c")
    endif()

    if(CONFIG_SHELL_CMD_EXEC)
        list(APPEND srcs "src/shell_exec.c")
    endif()

    if(CONFIG_SHELL_CMD_MOD_LOAD)
        list(APPEND srcs "src/shell_mod_load.c")
    endif()

    if(CONFIG_SHELL_CMD_LIST)
        list(APPEND srcs "src/shell_list.c")
    endif()

    if(CONFIG_SHELL_CMD_MOD_UNLOAD)
        list(APPEND srcs "src/shell_mod_unload.c")
    endif()

    if(CONFIG_SHELL_CMD_FREE)
        list(APPEND srcs "src/shell_free.c")
    endif()
endif()

idf_component_register(SRCS ${srcs}
                       INCLUDE_DIRS ${include_dir}
                       PRIV_INCLUDE_DIRS ${priv_include_dir}
                       REQUIRES "console" "elf_loader")
