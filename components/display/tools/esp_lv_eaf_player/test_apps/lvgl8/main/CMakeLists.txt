set(EAF_ASSETS_DIR "${CMAKE_BINARY_DIR}/eaf_assets")
set(EAF_LEGACY_EMBED "${EAF_ASSETS_DIR}/legacy.eaf")
set(EAF4_LOSSLESS_EMBED "${EAF_ASSETS_DIR}/lossless.eaf4")

file(MAKE_DIRECTORY ${EAF_ASSETS_DIR})
# Regenerate embedded fixtures when their generator changes during an incremental build.
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_LIST_DIR}/../../generate_test_assets.py")
execute_process(
    COMMAND "${PYTHON}" "${CMAKE_CURRENT_LIST_DIR}/../../generate_test_assets.py"
            "${EAF_LEGACY_EMBED}" "${EAF4_LOSSLESS_EMBED}"
    RESULT_VARIABLE EAF_ASSET_RESULT
)
if(NOT EAF_ASSET_RESULT EQUAL 0)
    message(FATAL_ERROR "Failed to generate self-contained EAF test assets")
endif()

idf_component_register(
    SRC_DIRS "."
    INCLUDE_DIRS "."
    EMBED_FILES "${EAF_LEGACY_EMBED}" "${EAF4_LOSSLESS_EMBED}")
