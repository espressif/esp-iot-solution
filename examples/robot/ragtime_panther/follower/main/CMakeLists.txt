idf_component_register(SRC_DIRS "." "ui" "ui/screens"
                    INCLUDE_DIRS "." "include" "ui" "ui/screens"
                    EMBED_FILES "../spiffs/merged-binary.bin")

if(TARGET build_receiver)
    add_dependencies(${COMPONENT_LIB} build_receiver)
endif()
