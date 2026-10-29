set(src_dirs        .)
set(include_dirs    . include)
set(requires        emotion_cls human_face_detect esp_painter)

idf_component_register(
    SRC_DIRS ${src_dirs}
    INCLUDE_DIRS ${include_dirs}
    REQUIRES ${requires}
)

target_compile_options(${COMPONENT_LIB} PRIVATE
    $<$<COMPILE_LANGUAGE:CXX>:-Wno-non-c-typedef-for-linkage>)

idf_component_get_property(esp-dl espressif__esp-dl COMPONENT_LIB)
target_compile_options(${esp-dl} PRIVATE
    -Wno-deprecated-declarations
    -Wno-missing-field-initializers
    $<$<COMPILE_LANGUAGE:CXX>:-Wno-non-c-typedef-for-linkage>)

idf_component_get_property(human-face-detect espressif__human_face_detect COMPONENT_LIB)
target_compile_options(${human-face-detect} PRIVATE
    $<$<COMPILE_LANGUAGE:CXX>:-Wno-non-c-typedef-for-linkage>)

idf_component_get_property(esp-video espressif__esp_video COMPONENT_LIB)
target_compile_options(${esp-video} PRIVATE
    -Wno-deprecated-declarations)
