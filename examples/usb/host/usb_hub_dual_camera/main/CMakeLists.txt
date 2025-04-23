idf_component_register(SRCS "usb_hub_dual_camera.c" "app_uvc.c" "app_https.c" "app_wifi.c"
                    INCLUDE_DIRS ".")
spiffs_create_partition_image(storage ../spiffs FLASH_IN_PROJECT)
