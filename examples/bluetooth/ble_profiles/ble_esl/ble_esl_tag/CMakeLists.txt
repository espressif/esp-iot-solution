cmake_minimum_required(VERSION 3.22)

list(APPEND EXTRA_COMPONENT_DIRS
     "${CMAKE_CURRENT_LIST_DIR}/../components/esl_product_common"
     "${CMAKE_CURRENT_LIST_DIR}/../components/esl_tag_hal"
     "${CMAKE_CURRENT_LIST_DIR}/../components/epaper"
     "${CMAKE_CURRENT_LIST_DIR}/../../../../../tools/cmake_utilities")

include($ENV{IDF_PATH}/tools/cmake/project.cmake)

set(BLE_ESL_SUPPORTED_TARGETS esp32h2 esp32h21 esp32h4 esp32c5 esp32c6 esp32c61 esp32s31)
if(NOT IDF_TARGET IN_LIST BLE_ESL_SUPPORTED_TARGETS)
    message(FATAL_ERROR
        "ble_esl_tag requires a PAwR-capable target (ESP32-H2/H21/H4, C5/C6/C61, S31); current: ${IDF_TARGET}")
endif()

project(ble_esl_tag)

# Image-store init aborts when the custom images partition is missing, and OTS
# rejects a 4736-byte SSD1680 frame when the object is smaller. CI only
# compiles, so fail here instead of on the first boot.
if(NOT CONFIG_PARTITION_TABLE_CUSTOM)
    message(FATAL_ERROR "ble_esl_tag requires CONFIG_PARTITION_TABLE_CUSTOM (images partition)")
endif()
if(NOT CONFIG_ESPTOOLPY_FLASHSIZE_4MB AND NOT CONFIG_ESPTOOLPY_FLASHSIZE_8MB
        AND NOT CONFIG_ESPTOOLPY_FLASHSIZE_16MB AND NOT CONFIG_ESPTOOLPY_FLASHSIZE_32MB
        AND NOT CONFIG_ESPTOOLPY_FLASHSIZE_64MB AND NOT CONFIG_ESPTOOLPY_FLASHSIZE_128MB)
    message(FATAL_ERROR "ble_esl_tag images partition needs at least 4MB flash")
endif()
if(CONFIG_BLE_ESL_MAX_IMAGES LESS 4)
    message(FATAL_ERROR "ble_esl_tag requires CONFIG_BLE_ESL_MAX_IMAGES >= 4")
endif()
if(CONFIG_BLE_ESL_MAX_IMAGE_SIZE LESS 4736)
    message(FATAL_ERROR "ble_esl_tag requires CONFIG_BLE_ESL_MAX_IMAGE_SIZE >= 4736")
endif()
if(NOT CONFIG_ESL_TAG_MAX_IMAGES EQUAL CONFIG_BLE_ESL_MAX_IMAGES)
    message(FATAL_ERROR "CONFIG_ESL_TAG_MAX_IMAGES must equal CONFIG_BLE_ESL_MAX_IMAGES")
endif()
if(NOT CONFIG_BT_NIMBLE_NVS_PERSIST)
    message(FATAL_ERROR "ble_esl_tag requires NimBLE bond persistence for reboot association restore")
endif()
if((IDF_TARGET STREQUAL "esp32c6" OR IDF_TARGET STREQUAL "esp32h4")
        AND NOT CONFIG_ESL_TAG_DISPLAY_BACKEND_EPAPER)
    message(FATAL_ERROR "ble_esl_tag ${IDF_TARGET} demo requires the SSD1680 e-paper backend")
endif()
if((IDF_TARGET STREQUAL "esp32c6" OR IDF_TARGET STREQUAL "esp32h4")
        AND NOT CONFIG_ESL_TAG_LED_BACKEND_NONE)
    message(FATAL_ERROR "ble_esl_tag ${IDF_TARGET} demo requires LED backend none (panel pin clash)")
endif()
