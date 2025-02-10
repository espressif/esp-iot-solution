idf_component_register(SRC_DIRS "."
                    INCLUDE_DIRS "." "include/")

idf_component_get_property(esp-dl espressif__esp-dl COMPONENT_LIB)
target_compile_options(${esp-dl} PRIVATE -Wno-unused-variable -Wno-unused-but-set-variable)

if(CONFIG_IDF_TARGET STREQUAL "esp32p4")
    set(CONFIG_MODEL_FILE_PATH "models/esp32p4/har.espdl")
elseif(CONFIG_IDF_TARGET STREQUAL "esp32s3")
    set(CONFIG_MODEL_FILE_PATH "models/esp32s3/har.espdl")
endif()
message("CONFIG_MODEL_FILE_PATH: ${CONFIG_MODEL_FILE_PATH}")
message("PROJECT_DIR: ${PROJECT_DIR}")


set(MVMODEL_EXE ${PROJECT_DIR}/pack_model.py)
idf_build_get_property(build_dir BUILD_DIR)
set(image_file ${build_dir}/espdl_models/models.espdl)
set(MODEL_FILE_PATH ${PROJECT_DIR}/${CONFIG_MODEL_FILE_PATH})

add_custom_command(
    OUTPUT ${image_file}
    COMMENT "Move and Pack models..."
    COMMAND python ${MVMODEL_EXE} --model_path=${MODEL_FILE_PATH} --out_file=${image_file}
    DEPENDS ${MODEL_FILE_PATH}
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
