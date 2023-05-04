idf_component_register(SRCS usb_audio_player_main.c
                    INCLUDE_DIRS ".")

spiffs_create_partition_image(storage ../spiffs FLASH_IN_PROJECT)
