idf_build_get_property(component_targets __COMPONENT_TARGETS)
if ("___idf_espressif__esp-dl" IN_LIST component_targets)
    idf_component_get_property(espdl_dir espressif__esp-dl COMPONENT_DIR)
elseif("___idf_esp-dl" IN_LIST component_targets)
    idf_component_get_property(espdl_dir esp-dl COMPONENT_DIR)
endif()
set(cmake_dir ${espdl_dir}/fbs_loader/cmake)
include(${cmake_dir}/utilities.cmake)

set(packed_model ${BUILD_DIR}/espdl_models/greencube_detect.espdl)
set(models ${COMPONENT_DIR}/models/p4/greencube_v2_224_esp32p4.espdl)

idf_component_register(SRCS "greencube_detect.cpp"
                       INCLUDE_DIRS "."
                       REQUIRES esp-dl)

file(MAKE_DIRECTORY ${BUILD_DIR}/espdl_models)
set(pack_model_exe ${espdl_dir}/fbs_loader/pack_espdl_models.py)
add_custom_command(
    OUTPUT ${packed_model}
    COMMENT "Pack greencube_detect model into flash rodata..."
    COMMAND python ${pack_model_exe} --model_path ${models} --out_file ${packed_model}
    DEPENDS ${models}
    VERBATIM)
target_add_aligned_binary_data(${COMPONENT_LIB} ${packed_model} BINARY)
