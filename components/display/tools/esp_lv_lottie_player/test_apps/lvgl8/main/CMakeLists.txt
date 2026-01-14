set(LOTTIE_URL "https://dl.espressif.com/AE/esp-dev-kits/welcome.json")
set(LOTTIE_FILE_NAME "welcome.json")
set(LOTTIE_ASSETS_DIR "${CMAKE_BINARY_DIR}/lottie_assets")
set(LOTTIE_EMBED_FILE "${LOTTIE_ASSETS_DIR}/${LOTTIE_FILE_NAME}")

file(MAKE_DIRECTORY ${LOTTIE_ASSETS_DIR})
message(STATUS "Downloading lottie asset from ${LOTTIE_URL}")
file(DOWNLOAD ${LOTTIE_URL} ${LOTTIE_EMBED_FILE} SHOW_PROGRESS)

idf_component_register(
    SRCS "test_lottie_player.c"
    INCLUDE_DIRS "."
    EMBED_TXTFILES "${LOTTIE_EMBED_FILE}")
