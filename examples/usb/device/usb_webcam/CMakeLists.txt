cmake_minimum_required(VERSION 3.5)

include($ENV{IDF_PATH}/tools/cmake/project.cmake)

# check if the esp32-s3-eye is chosen
if("$ENV{IDF_TARGET}" STREQUAL "esp32s3")
  if(EXISTS ${CMAKE_CURRENT_LIST_DIR}/sdkconfig)
    file(READ ${CMAKE_CURRENT_LIST_DIR}/sdkconfig SDKCONFIG_RULE)
    string(REGEX MATCH "CONFIG_CAMERA_MODULE_ESP_S3_EYE=y" ENABLE_ESP_S3_EYE "${SDKCONFIG_RULE}")
  else()
    message(STATUS "sdkconfig not exist")
    file(READ ${CMAKE_CURRENT_LIST_DIR}/main/Kconfig.projbuild SDKCONFIG_RULE)
    string(REGEX MATCH "default CAMERA_MODULE_ESP_S3_EYE" ENABLE_ESP_S3_EYE "${SDKCONFIG_RULE}")
  endif()
endif()

if (ENABLE_ESP_S3_EYE)
  set(ENV{USB_WEBCAM_BOARD} "esp32_s3_eye")
else()
  set(ENV{USB_WEBCAM_BOARD} "general")
endif()

message("USB_WEBCAM_BOARD is set to $ENV{USB_WEBCAM_BOARD}")
project(usb_webcam)
