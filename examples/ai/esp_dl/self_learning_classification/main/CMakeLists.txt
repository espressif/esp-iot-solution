idf_component_register(SRC_DIRS "." "ui" "ui/screens"
                    INCLUDE_DIRS "." "include" "ui" "ui/screens")

idf_component_get_property(esp-dl espressif__esp-dl COMPONENT_LIB)
target_compile_options(${esp-dl} PRIVATE
    -Wno-deprecated-declarations
    -Wno-missing-field-initializers)

idf_component_get_property(esp-video espressif__esp_video COMPONENT_LIB)
target_compile_options(${esp-video} PRIVATE
    -Wno-deprecated-declarations)
