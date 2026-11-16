set(EAF_ASSETS_DIR "${CMAKE_BINARY_DIR}/eaf_assets")
set(EAF_LEGACY_EMBED "${EAF_ASSETS_DIR}/legacy.eaf")
set(EAF4_LOSSLESS_EMBED "${EAF_ASSETS_DIR}/lossless.eaf4")
set(EAF_JPEG_EMBED "${EAF_ASSETS_DIR}/jpeg.eaf")
set(EAF_UNALIGNED_EMBED "${EAF_ASSETS_DIR}/unaligned.eaf")
set(EAF_DIRECT_EMBED "${EAF_ASSETS_DIR}/direct.eaf")
file(MAKE_DIRECTORY ${EAF_ASSETS_DIR})
# Regenerate embedded fixtures when their generator changes during an incremental build.
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_LIST_DIR}/../../generate_test_assets.py")
execute_process(
    COMMAND "${PYTHON}" "${CMAKE_CURRENT_LIST_DIR}/../../generate_test_assets.py"
            "${EAF_LEGACY_EMBED}" "${EAF4_LOSSLESS_EMBED}" "${EAF_JPEG_EMBED}" "${EAF_UNALIGNED_EMBED}" "${EAF_DIRECT_EMBED}"
    RESULT_VARIABLE EAF_ASSET_RESULT
)
if(NOT EAF_ASSET_RESULT EQUAL 0)
    message(FATAL_ERROR "Failed to generate EAF core test assets")
endif()

set(EAF_TEST_SOURCES "test_eaf_core.c")
set(EAF_TEST_EMBED_FILES "${EAF_LEGACY_EMBED}" "${EAF4_LOSSLESS_EMBED}"
    "${EAF_JPEG_EMBED}" "${EAF_UNALIGNED_EMBED}" "${EAF_DIRECT_EMBED}")
if(CONFIG_EAF_TEST_ALL_MODES)
    set(EAF_TEST_FIXTURE_PYTHON "${PYTHON}" CACHE FILEPATH "Python with NumPy/Pillow for all-mode fixtures")
    set(EAF_ALL_MODES "${EAF_ASSETS_DIR}/all_modes.bin")
    file(GLOB_RECURSE EAF_ENCODER_INPUTS "${CMAKE_CURRENT_LIST_DIR}/../../../encoder/*.py")
    set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS
        "${CMAKE_CURRENT_LIST_DIR}/../../generate_all_modes.py"
        "${CMAKE_CURRENT_LIST_DIR}/../../host/jpeg_fixtures.py" ${EAF_ENCODER_INPUTS})
    execute_process(COMMAND "${EAF_TEST_FIXTURE_PYTHON}"
        "${CMAKE_CURRENT_LIST_DIR}/../../generate_all_modes.py" "${EAF_ALL_MODES}"
        RESULT_VARIABLE EAF_ALL_RESULT)
    if(NOT EAF_ALL_RESULT EQUAL 0)
        message(FATAL_ERROR "All-mode generation failed; install component requirements in EAF_TEST_FIXTURE_PYTHON")
    endif()
    list(APPEND EAF_TEST_SOURCES "test_all_modes.c")
    list(APPEND EAF_TEST_EMBED_FILES "${EAF_ALL_MODES}")
endif()
idf_component_register(SRCS ${EAF_TEST_SOURCES} INCLUDE_DIRS "." EMBED_FILES ${EAF_TEST_EMBED_FILES})
