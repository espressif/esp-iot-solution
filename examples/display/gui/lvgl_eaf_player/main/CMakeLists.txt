set(srcs "main.c")
set(include_dirs ".")

idf_component_register(
    SRCS ${srcs}
    INCLUDE_DIRS ${include_dirs}
    REQUIRES hw_init
)

set(EAF_URL "https://dl.espressif.com/AE/esp-dev-kits/eaf_test.eaf")
set(EAF_FILE "eaf_test.eaf")
set(EAF_ASSETS_DIR "${CMAKE_BINARY_DIR}/eaf_assets")
set(EAF_DOWNLOADED_FILE "${EAF_ASSETS_DIR}/${EAF_FILE}")

file(MAKE_DIRECTORY ${EAF_ASSETS_DIR})
message(STATUS "Downloading EAF asset from ${EAF_URL}")
file(DOWNLOAD ${EAF_URL} ${EAF_DOWNLOADED_FILE} SHOW_PROGRESS)

if(EXISTS "${EAF_DOWNLOADED_FILE}")
    spiffs_create_partition_assets(eaf "${EAF_ASSETS_DIR}" FLASH_IN_PROJECT
        MMAP_FILE_SUPPORT_FORMAT ".eaf"
        IMPORT_INC_PATH "${CMAKE_CURRENT_SOURCE_DIR}")
    add_dependencies(${COMPONENT_LIB} assets_eaf_bin)
endif()
