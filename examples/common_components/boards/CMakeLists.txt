
set(boards_dir "")

if(CONFIG_BOARD_ESP32_DEVKITC_V4)
    set(boards_dir "esp32-devkitc-v4")
elseif(CONFIG_BOARD_ESP32_MESHKIT_SENSE)
    set(boards_dir "esp32-meshkit-sense")
elseif(CONFIG_BOARD_ESP32S2_SAOLA_1)
    set(boards_dir "esp32s2-saola-1")
elseif(CONFIG_BOARD_ESP32_LCDKIT)
    set(boards_dir "esp32-lcdkit")
elseif(CONFIG_BOARD_ESP32S3_USB_OTG_EV)
    set(boards_dir "esp32s3_usb_otg_ev")
endif()

message(STATUS "-----------Board Info---------")
message(STATUS "IDF_TARGET = ${IDF_TARGET}")
message(STATUS "Board DIR = ${boards_dir}")
message(STATUS "---------Board Info End---------")

#The SRC_DIRS order cannot be changed, otherwise weak functions may not be overridden
idf_component_register( SRC_DIRS "${boards_dir}" "."
                        INCLUDE_DIRS "${boards_dir}" "."
                        REQUIRES bus button esp_adc_cal nvs_flash)
