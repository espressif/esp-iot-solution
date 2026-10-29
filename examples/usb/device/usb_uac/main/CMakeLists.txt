idf_component_register(SRC_DIRS "."
                       INCLUDE_DIRS ".")

idf_build_get_property(build_components BUILD_COMPONENTS)
if(espressif__esp_video IN_LIST build_components)
    idf_component_get_property(esp_video_lib espressif__esp_video COMPONENT_LIB)
    target_compile_options(${esp_video_lib} PRIVATE -Wno-deprecated-declarations)
endif()
