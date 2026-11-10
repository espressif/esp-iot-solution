if(NOT TARGET gen_diff_ota)
    add_custom_target(gen_diff_ota)

    if(CONFIG_SECURE_SIGNED_ON_UPDATE_NO_SECURE_BOOT OR CONFIG_SECURE_BOOT_V2_ENABLED)
        set(DIFF_OTA_BIN_SIGN_PARA --sign_key ${PROJECT_DIR}/${CONFIG_SECURE_BOOT_SIGNING_KEY})
    else()
        set(DIFF_OTA_BIN_SIGN_PARA)
    endif()

    if(CONFIG_CU_OTA_PAD_HTTPS)
        set(DIFF_OTA_PAD_PARA --pad_https_ota)
    else()
        set(DIFF_OTA_PAD_PARA)
    endif()

    # This target builds the project and uses that binary as the base.
    # The new firmware is a separate file: the build overwrites build/<project>.bin.
    idf_build_get_property(_diff_ota_elf_name EXECUTABLE_NAME)
    if(NOT _diff_ota_elf_name)
        set(_diff_ota_elf_name "${CMAKE_PROJECT_NAME}")
    endif()
    set(DIFF_OTA_BASE_ABS "${CMAKE_BINARY_DIR}/${_diff_ota_elf_name}.bin")

    if(DIFF_OTA_NEW_FILE)
        get_filename_component(DIFF_OTA_NEW_ABS "${DIFF_OTA_NEW_FILE}" ABSOLUTE BASE_DIR "${PROJECT_DIR}")
    else()
        set(DIFF_OTA_NEW_ABS "")
    endif()

    add_custom_command(TARGET gen_diff_ota
        POST_BUILD
        COMMAND ${CMAKE_COMMAND}
                -DBASE=${DIFF_OTA_BASE_ABS}
                -DNEW=${DIFF_OTA_NEW_ABS}
                -P ${CMAKE_CURRENT_LIST_DIR}/scripts/require_diff_bins.cmake
        COMMAND ${PYTHON} ${CMAKE_CURRENT_LIST_DIR}/scripts/gen_custom_ota.py
                -hv v3 -c xz -d ddelta --add_app_header
                -b ${DIFF_OTA_BASE_ABS}
                -i ${DIFF_OTA_NEW_ABS}
                ${DIFF_OTA_BIN_SIGN_PARA}
                ${DIFF_OTA_PAD_PARA}
        COMMENT "Generating ddelta patch from ${DIFF_OTA_BASE_ABS} to ${DIFF_OTA_NEW_ABS}"
        VERBATIM
    )
    add_dependencies(gen_diff_ota gen_project_binary)
endif()
