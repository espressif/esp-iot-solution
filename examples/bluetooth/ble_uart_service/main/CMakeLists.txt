set(MAIN_SRCS
    "app_main.c"
    "board.c"
    "emote_demo.c"
    "ble_protocol.c"
)

file(GLOB MAIN_ASSET_SRCS "assets/*.c")
list(APPEND MAIN_SRCS ${MAIN_ASSET_SRCS})

idf_component_register(
    SRCS ${MAIN_SRCS}
    INCLUDE_DIRS "."
    PRIV_REQUIRES ble_uart bt nvs_flash cjson touch_slider_sensor touch_button
)

set(EMOTE_ASSETS_URL "https://dl.espressif.com/AE/emote_assets.bin")
set(EMOTE_ASSETS_BIN "${CMAKE_BINARY_DIR}/prebuilt/emote_assets.bin")

if(NOT EXISTS "${EMOTE_ASSETS_BIN}")
    file(MAKE_DIRECTORY "${CMAKE_BINARY_DIR}/prebuilt")
    message(STATUS "Downloading emote_assets.bin from ${EMOTE_ASSETS_URL}")
    file(DOWNLOAD "${EMOTE_ASSETS_URL}" "${EMOTE_ASSETS_BIN}"
        SHOW_PROGRESS
        STATUS _emote_assets_dl_status)
    list(GET _emote_assets_dl_status 0 _emote_assets_dl_code)
    if(NOT _emote_assets_dl_code EQUAL 0)
        list(GET _emote_assets_dl_status 1 _emote_assets_dl_msg)
        message(FATAL_ERROR
            "Failed to download emote_assets.bin: ${_emote_assets_dl_msg} (code ${_emote_assets_dl_code})")
    endif()
endif()

set(DIR_ASSETS_GEN "${CMAKE_CURRENT_LIST_DIR}/../assets_gen")
spiffs_create_partition_assets(
    emote_gen
    ${DIR_ASSETS_GEN}
    FLASH_IN_PROJECT
    COPY_PREBUILT_BIN ${EMOTE_ASSETS_BIN}
)
