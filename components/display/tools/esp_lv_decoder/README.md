[![Component Registry](https://components.espressif.com/components/espressif/esp_lv_decoder/badge.svg)](https://components.espressif.com/components/espressif/esp_lv_decoder)

## Instructions and Details

Allow the use of images in LVGL. Besides that it also allows the use of a custom format, called Split image, which can be decoded in more optimal way on embedded systems.

[Referencing the implementation of SJPG.](https://docs.lvgl.io/8.4/libs/sjpg.html#overview)

### Features
    - Supports both standard and custom split image formats, including JPG, PNG, and QOI.

    - Decoding standard image requires RAM equivalent to the full uncompressed image size (recommended for devices with more RAM).

    - Split image is a custom format based on standard image formats, specifically designed for LVGL.

    - File reads are implemented for both file storage and C-arrays.

    - Split images are decoded in segments, so zooming and rotating are not supported.


## Add to project

Packages from this repository are uploaded to [Espressif's component service](https://components.espressif.com/).
You can add them to your project via `idf.py add-dependancy`, e.g.
```
    idf.py add-dependency esp_lv_decoder
```

## Usage
The [esp_mmap_assets](https://components.espressif.com/components/espressif/esp_mmap_assets) component is required. It automatically packages and converts images to your desired format during compilation.

### Converting JPG to SJPG
#### CMake
```c
    spiffs_create_partition_assets(
        my_spiffs_partition
        my_folder
        FLASH_IN_PROJECT
        MMAP_FILE_SUPPORT_FORMAT ".jpg"
        MMAP_SUPPORT_SJPG
        MMAP_SPLIT_HEIGHT 16)
```
#### Output log
```c
    [5/21] Move and Pack assets...
    --support_format: ['.jpg']
    --support_spng: False
    --support_sjpg: True
    --support_qoi: False
    --support_raw: False
    --split_height: 16
    Completed navi_52.jpg -> navi_52.sjpg
```

### Converting PNG to SPNG
#### CMake
```c
    spiffs_create_partition_assets(
        my_spiffs_partition
        my_folder
        FLASH_IN_PROJECT
        MMAP_FILE_SUPPORT_FORMAT ".png"
        MMAP_SUPPORT_SPNG
        MMAP_SPLIT_HEIGHT 16)
```
#### Output log
```c
    [9/20] Move and Pack assets...
    --support_format: ['.png']
    --support_spng: True
    --support_sjpg: False
    --support_qoi: False
    --support_raw: False
    --split_height: 16
    Completed navi_52.png -> navi_52.spng
```

### Converting PNG、JPG to QOI
#### CMake
```c
    spiffs_create_partition_assets(
        my_spiffs_partition
        my_folder
        FLASH_IN_PROJECT
        MMAP_FILE_SUPPORT_FORMAT ".jpg,.png"
        MMAP_SUPPORT_QOI)
```
#### Output log
```c
    [12/20] Move and Pack assets...
    --support_format: ['.jpg', '.png']
    --support_spng: False
    --support_sjpg: False
    --support_qoi: True
    --support_raw: False
    Completed navi_52.png -> navi_52.qoi
    Completed navi_53.jpg -> navi_53.qoi
```

### Converting PNG、JPG to SQOI
#### CMake
```c
    spiffs_create_partition_assets(
        my_spiffs_partition
        my_folder
        FLASH_IN_PROJECT
        MMAP_FILE_SUPPORT_FORMAT ".jpg,.png"
        MMAP_SUPPORT_QOI
        MMAP_SUPPORT_SQOI
        MMAP_SPLIT_HEIGHT 16)
```
#### Output log
```c
    --support_format: ['.jpg', '.png']
    --support_spng: False
    --support_sjpg: False
    --support_qoi: True
    --support_raw: False
    --support_sqoi: True
    --split_height: 16
    Completed navi_52.png -> navi_52.sqoi
    Completed navi_53.jpg -> navi_53.sqoi
```

### Initialization
```c
    esp_lv_decoder_handle_t decoder_handle = NULL;
    esp_lv_decoder_init(&decoder_handle); //Initialize this after lvgl starts
```
