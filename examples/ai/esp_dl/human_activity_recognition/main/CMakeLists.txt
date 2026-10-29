idf_component_register(SRC_DIRS "."
                    INCLUDE_DIRS "." "include/")

target_compile_options(${COMPONENT_LIB} PRIVATE
    $<$<COMPILE_LANGUAGE:CXX>:-Wno-non-c-typedef-for-linkage>)

idf_component_get_property(esp-dl espressif__esp-dl COMPONENT_LIB)
target_compile_options(${esp-dl} PRIVATE
    -Wno-unused-variable
    -Wno-unused-but-set-variable
    -Wno-deprecated-declarations
    -Wno-missing-field-initializers
    $<$<COMPILE_LANGUAGE:CXX>:-Wno-non-c-typedef-for-linkage>)

# Locate esp-dl and reuse its packing helpers (no local pack_model.py needed).
idf_build_get_property(component_targets __COMPONENT_TARGETS)
if ("___idf_espressif__esp-dl" IN_LIST component_targets)
    idf_component_get_property(espdl_dir espressif__esp-dl COMPONENT_DIR)
elseif("___idf_esp-dl" IN_LIST component_targets)
    idf_component_get_property(espdl_dir esp-dl COMPONENT_DIR)
endif()
include(${espdl_dir}/fbs_loader/cmake/utilities.cmake)

if(IDF_TARGET STREQUAL "esp32s3")
    set(models_dir ${PROJECT_DIR}/models/esp32s3)
elseif(IDF_TARGET STREQUAL "esp32p4" OR IDF_TARGET STREQUAL "esp32s31")
    set(models_dir ${PROJECT_DIR}/models/esp32p4)
endif()
message("models_dir: ${models_dir}")

set(model_file ${models_dir}/har.espdl)
idf_build_get_property(build_dir BUILD_DIR)
set(image_file ${build_dir}/espdl_models/models.espdl)
file(MAKE_DIRECTORY ${build_dir}/espdl_models)

set(pack_model_exe ${espdl_dir}/fbs_loader/pack_espdl_models.py)
add_custom_command(
    OUTPUT ${image_file}
    COMMENT "Move and Pack models..."
    COMMAND python ${pack_model_exe} --model_path ${model_file} --out_file ${image_file} --format PDL2
    DEPENDS ${model_file}
    VERBATIM)

add_custom_target(models_espdl ALL DEPENDS ${image_file})
add_dependencies(flash models_espdl)

partition_table_get_partition_info(size "--partition-name model" "size")
partition_table_get_partition_info(offset "--partition-name model" "offset")

if("${size}" AND "${offset}")
    esptool_py_flash_to_partition(flash "model" "${image_file}")
else()
    set(message "Failed to find model in partition table file"
                "Please add a line(Name=model, Size>recommended size in log) to the partition file.")
endif()
