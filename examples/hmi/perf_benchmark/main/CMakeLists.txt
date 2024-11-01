
idf_component_register(
    SRC_DIRS "."
    INCLUDE_DIRS ".")

set(DIR_JPG "../assets_jpg")
set(DIR_PNG "../assets_png")

set(Drive_PNG_PNG   "${CMAKE_BINARY_DIR}/Drive_A")
set(Drive_PNG_SPNG  "${CMAKE_BINARY_DIR}/Drive_B")
set(Drive_PNG_QOI   "${CMAKE_BINARY_DIR}/Drive_C")
set(Drive_PNG_SQOI  "${CMAKE_BINARY_DIR}/Drive_D")
set(Drive_PNG_RAW   "${CMAKE_BINARY_DIR}/Drive_E")

set(Drive_JPG_JPG   "${CMAKE_BINARY_DIR}/Drive_F")
set(Drive_JPG_SJPG  "${CMAKE_BINARY_DIR}/Drive_G")
set(Drive_JPG_QOI   "${CMAKE_BINARY_DIR}/Drive_H")
set(Drive_JPG_SQOI  "${CMAKE_BINARY_DIR}/Drive_I")
set(Drive_JPG_RAW   "${CMAKE_BINARY_DIR}/Drive_J")

file(MAKE_DIRECTORY ${Drive_PNG_PNG})
file(MAKE_DIRECTORY ${Drive_PNG_SPNG})
file(MAKE_DIRECTORY ${Drive_PNG_QOI})
file(MAKE_DIRECTORY ${Drive_PNG_SQOI})
file(MAKE_DIRECTORY ${Drive_PNG_RAW})

file(MAKE_DIRECTORY ${Drive_JPG_JPG})
file(MAKE_DIRECTORY ${Drive_JPG_SJPG})
file(MAKE_DIRECTORY ${Drive_JPG_QOI})
file(MAKE_DIRECTORY ${Drive_JPG_SQOI})
file(MAKE_DIRECTORY ${Drive_JPG_RAW})

file(GLOB_RECURSE SOURCE_FILES ${DIR_PNG}/*)
foreach(FILE ${SOURCE_FILES})
    file(COPY ${FILE} DESTINATION ${Drive_PNG_PNG})
    file(COPY ${FILE} DESTINATION ${Drive_PNG_SPNG})
    file(COPY ${FILE} DESTINATION ${Drive_PNG_QOI})
    file(COPY ${FILE} DESTINATION ${Drive_PNG_SQOI})
    file(COPY ${FILE} DESTINATION ${Drive_PNG_RAW})
endforeach()

file(GLOB_RECURSE SOURCE_FILES ${DIR_JPG}/*)
foreach(FILE ${SOURCE_FILES})
    file(COPY ${FILE} DESTINATION ${Drive_JPG_JPG})
    file(COPY ${FILE} DESTINATION ${Drive_JPG_SJPG})
    file(COPY ${FILE} DESTINATION ${Drive_JPG_QOI})
    file(COPY ${FILE} DESTINATION ${Drive_JPG_SQOI})
    file(COPY ${FILE} DESTINATION ${Drive_JPG_RAW})
endforeach()

spiffs_create_partition_assets(
    assets_A
    ${Drive_PNG_PNG}
    FLASH_IN_PROJECT
    MMAP_FILE_SUPPORT_FORMAT ".png"
)
spiffs_create_partition_assets(
    assets_B
    ${Drive_PNG_SPNG}
    FLASH_IN_PROJECT
    MMAP_FILE_SUPPORT_FORMAT ".png"
    MMAP_SUPPORT_SPNG
    MMAP_SPLIT_HEIGHT 16
)
spiffs_create_partition_assets(
    assets_C
    ${Drive_PNG_QOI}
    FLASH_IN_PROJECT
    MMAP_FILE_SUPPORT_FORMAT ".png"
    MMAP_SUPPORT_QOI
)
spiffs_create_partition_assets(
    assets_D
    ${Drive_PNG_SQOI}
    FLASH_IN_PROJECT
    MMAP_FILE_SUPPORT_FORMAT ".png"
    MMAP_SUPPORT_QOI
    MMAP_SUPPORT_SQOI
    MMAP_SPLIT_HEIGHT 16
)
spiffs_create_partition_assets(
    assets_E
    ${Drive_PNG_RAW}
    FLASH_IN_PROJECT
    MMAP_FILE_SUPPORT_FORMAT ".png"
    MMAP_SUPPORT_RAW
    MMAP_RAW_DITHER
    MMAP_RAW_BGR_MODE
    MMAP_RAW_FILE_FORMAT "true_color_alpha"
    MMAP_RAW_COLOR_FORMAT "RGB565SWAP"
)

spiffs_create_partition_assets(
    assets_F
    ${Drive_JPG_JPG}
    FLASH_IN_PROJECT
    MMAP_FILE_SUPPORT_FORMAT ".jpg"
)
spiffs_create_partition_assets(
    assets_G
    ${Drive_JPG_SJPG}
    FLASH_IN_PROJECT
    MMAP_FILE_SUPPORT_FORMAT ".jpg"
    MMAP_SUPPORT_SJPG
    MMAP_SPLIT_HEIGHT 16
)
spiffs_create_partition_assets(
    assets_H
    ${Drive_JPG_QOI}
    FLASH_IN_PROJECT
    MMAP_FILE_SUPPORT_FORMAT ".jpg"
    MMAP_SUPPORT_QOI
)
spiffs_create_partition_assets(
    assets_I
    ${Drive_JPG_SQOI}
    FLASH_IN_PROJECT
    MMAP_FILE_SUPPORT_FORMAT ".jpg"
    MMAP_SUPPORT_QOI
    MMAP_SUPPORT_SQOI
    MMAP_SPLIT_HEIGHT 16
)
spiffs_create_partition_assets(
    assets_J
    ${Drive_JPG_RAW}
    FLASH_IN_PROJECT
    MMAP_FILE_SUPPORT_FORMAT ".jpg"
    MMAP_SUPPORT_RAW
    MMAP_RAW_DITHER
    MMAP_RAW_BGR_MODE
    MMAP_RAW_FILE_FORMAT "true_color"
    MMAP_RAW_COLOR_FORMAT "RGB565SWAP"
)
