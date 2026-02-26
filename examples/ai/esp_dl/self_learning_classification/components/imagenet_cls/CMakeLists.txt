idf_build_get_property(component_targets __COMPONENT_TARGETS)
if ("___idf_espressif__esp-dl" IN_LIST component_targets)
    idf_component_get_property(espdl_dir espressif__esp-dl COMPONENT_DIR)
elseif("___idf_esp-dl" IN_LIST component_targets)
    idf_component_get_property(espdl_dir esp-dl COMPONENT_DIR)
endif()
set(cmake_dir ${espdl_dir}/fbs_loader/cmake)
include(${cmake_dir}/utilities.cmake)

set(src_dirs        .)

set(include_dirs    .)

set(requires        esp-dl)

set(packed_model ${BUILD_DIR}/espdl_models/imagenet_cls.espdl)

idf_component_register(SRC_DIRS ${src_dirs} INCLUDE_DIRS ${include_dirs} REQUIRES ${requires})

if (NOT CONFIG_IMAGENET_CLS_MODEL_IN_SDCARD)
    if (IDF_TARGET STREQUAL "esp32p4")
        set(models_dir ${COMPONENT_DIR}/model/p4)
    endif()

    file(MAKE_DIRECTORY ${BUILD_DIR}/espdl_models)
    set(models)
    if(CONFIG_FLASH_IMAGENET_CLS_MOBILENETV2_S8_V1)
        list(APPEND models ${models_dir}/imagenet_cls_mobilenetv2_s8_v1.espdl)
    endif()

    set(pack_model_exe ${espdl_dir}/fbs_loader/pack_espdl_models.py)
    add_custom_command(
        OUTPUT ${packed_model}
        COMMENT "Move and Pack models..."
        COMMAND python ${pack_model_exe} --model_path ${models} --out_file ${packed_model}
        DEPENDS ${models}
        VERBATIM)

    if(CONFIG_IMAGENET_CLS_MODEL_IN_FLASH_RODATA)
        target_add_aligned_binary_data(${COMPONENT_LIB} ${packed_model} BINARY)
    endif()

    if(CONFIG_IMAGENET_CLS_MODEL_IN_FLASH_PARTITION)
        add_custom_target(imagenet_cls_model ALL DEPENDS ${packed_model})
        add_dependencies(flash imagenet_cls_model)
        esptool_py_flash_to_partition(flash "imagenet_cls" ${packed_model})
    endif()
endif()
