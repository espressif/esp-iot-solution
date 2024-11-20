# The following lines of boilerplate have to be in your project's
# CMakeLists in this exact order for cmake to work correctly
cmake_minimum_required(VERSION 3.16)
include($ENV{IDF_PATH}/tools/cmake/project.cmake)

set(EXTRA_COMPONENT_DIRS
        ../../esp_modem_usb_dte
        ../../usb_host_cdc_acm
        ../../usb_host_ch34x_vcp
        ../../usb_host_cp210x_vcp
        ../../usb_host_ftdi_vcp
        ../../usb_host_vcp
        )

# Set the components to include the tests for.
if("${IDF_VERSION_MAJOR}.${IDF_VERSION_MINOR}" VERSION_GREATER_EQUAL "5.0")
    list(APPEND EXTRA_COMPONENT_DIRS ../../../../../device/esp_tinyusb)
endif()

# "Trim" the build. Include the minimal set of components, main, and anything it depends on.
set(COMPONENTS main)

project(test_app_usb_host_cdc)
