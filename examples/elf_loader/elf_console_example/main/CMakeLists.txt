set(srcs "elf_console_example")

idf_component_register(SRCS "${srcs}"
                       REQUIRES nvs_flash)

# Create a SPIFFS image from the contents of the 'spiffs_image' directory
# that fits the partition named 'storage'. It's not friendly to flash the
# generated image with 'idf.py -p PORT flash', since the image will be
# flashed each time, but you can use 'idf.py -p PORT storage-flash' to
# flash this image separately.
littlefs_create_partition_image(storage fs_image)
