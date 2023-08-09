cmake_minimum_required(VERSION 3.5)

include($ENV{IDF_PATH}/tools/cmake/project.cmake)

# check if the esp32-s3-eye is chosen
if("$ENV{IDF_TARGET}" STREQUAL "esp32s3")
  if(EXISTS ${CMAKE_CURRENT_LIST_DIR}/sdkconfig)
    file(READ ${CMAKE_CURRENT_LIST_DIR}/sdkconfig SDKCONFIG_RULE)
    string(REGEX MATCH "CONFIG_CAMERA_MODULE_ESP_S3_EYE=y" ENABLE_EYES_SHOW "${SDKCONFIG_RULE}")
  else()
    message(STATUS "sdkconfig not exist")
    file(READ ${CMAKE_CURRENT_LIST_DIR}/main/Kconfig.projbuild SDKCONFIG_RULE)
    string(REGEX MATCH "default CAMERA_MODULE_ESP_S3_EYE" ENABLE_EYES_SHOW "${SDKCONFIG_RULE}")
  endif()
endif()

if (ENABLE_EYES_SHOW)
  set(ENV{EYES_SHOW_TARGET} "esp32s3")
  message("EYES_SHOW_TARGET is set to esp32s3")
  else()
  set(ENV{EYES_SHOW_TARGET} "none")
  message("EYES_SHOW_TARGET is set to none")
endif()

project(usb_webcam)
