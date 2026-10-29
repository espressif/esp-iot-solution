set(EAF_URL "https://dl.espressif.com/AE/esp-dev-kits/eaf_test.eaf")
set(EAF_FILE_NAME "test.eaf")
set(EAF_ASSETS_DIR "${CMAKE_BINARY_DIR}/eaf_assets")
set(EAF_EMBED_FILE "${EAF_ASSETS_DIR}/${EAF_FILE_NAME}")

file(MAKE_DIRECTORY ${EAF_ASSETS_DIR})
message(STATUS "Downloading EAF asset from ${EAF_URL}")
file(DOWNLOAD ${EAF_URL} ${EAF_EMBED_FILE} SHOW_PROGRESS)

idf_component_register(
    SRC_DIRS "."
    INCLUDE_DIRS "."
    EMBED_TXTFILES "${EAF_EMBED_FILE}")
