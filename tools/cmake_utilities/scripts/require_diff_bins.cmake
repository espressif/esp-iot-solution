if(NOT DEFINED BASE OR BASE STREQUAL "" OR NOT EXISTS "${BASE}")
    message(FATAL_ERROR
        "gen_diff_ota builds this project and uses that binary as the base, but the base file is missing.\n"
        "Base got: '${BASE}'")
endif()
if(NOT DEFINED NEW OR NEW STREQUAL "" OR NOT EXISTS "${NEW}")
    message(FATAL_ERROR
        "gen_diff_ota builds the base from this project. Pass the new firmware separately.\n"
        "From the project directory:\n"
        "  idf.py -DDIFF_OTA_NEW_FILE=backup/simple_ota.bin gen_diff_ota\n"
        "The path is relative to the project directory unless absolute.\n"
        "Copy the new firmware aside before this command. The build overwrites build/<project>.bin.\n"
        "New got: '${NEW}'")
endif()
