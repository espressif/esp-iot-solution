if(CONFIG_BOARD_TARGET_PATH)
    set(boards_dir ${CONFIG_BOARD_TARGET_PATH} )
else()
    message(FATAL_ERROR, "Board path not exists")
endif()


if("${IDF_VERSION_MAJOR}.${IDF_VERSION_MINOR}" VERSION_GREATER_EQUAL "5.0")
    list(APPEND PRIVREQ esp_adc)
else()
    list(APPEND PRIVREQ esp_adc_cal)
endif()

set(user_src_dir "")
if(DEFINED boards_dir)
message(STATUS "-----------Board Info---------")
message(STATUS "IDF_TARGET = ${IDF_TARGET}")
message(STATUS "Board DIR = ${boards_dir}")
message(STATUS "---------Board Info End---------")
get_filename_component(abs_dir ${boards_dir} ABSOLUTE)
if(IS_DIRECTORY ${abs_dir})
file(GLOB dir_sources "${abs_dir}/*.c")
if(dir_sources)
    set(user_src_dir ${abs_dir})
    message(STATUS "source files found for '${boards_dir}'.")
else()
    message(STATUS "No source files found for '${boards_dir}'.")
endif()
endif()
endif()

#The SRC_DIRS order cannot be changed, otherwise weak functions may not be overridden
idf_component_register( SRC_DIRS "${user_src_dir}" "."
                        INCLUDE_DIRS "${boards_dir}" "."
                        REQUIRES ${PRIVREQ} nvs_flash esp_wifi)
