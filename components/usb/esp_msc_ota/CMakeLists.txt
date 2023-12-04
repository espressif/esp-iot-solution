idf_component_register(SRC_DIRS "."
                       INCLUDE_DIRS "." "include"
                       REQUIRES esp_event bootloader_support esp_app_format usb fatfs app_update vfs)
