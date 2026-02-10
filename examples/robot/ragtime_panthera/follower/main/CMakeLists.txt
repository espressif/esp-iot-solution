set(embed_files "")

if(EXISTS "../spiffs/merged-binary.bin")
    list(APPEND embed_files "../spiffs/merged-binary.bin")
elseif(DEFINED ENV{SKIP_RECEIVER_BUILD})
    message(WARNING "Please build the receiver firmware first")
endif()

idf_component_register(SRC_DIRS "." "ui" "ui/screens"
                    INCLUDE_DIRS "." "include" "ui" "ui/screens"
                    EMBED_FILES ${embed_files})

if(TARGET build_receiver)
    add_dependencies(${COMPONENT_LIB} build_receiver)
endif()
