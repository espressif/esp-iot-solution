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

### Converting PNG to PJPG
#### CMake
```c
    spiffs_create_partition_assets(
        my_spiffs_partition
        my_folder
        FLASH_IN_PROJECT
        MMAP_FILE_SUPPORT_FORMAT ".png"
        MMAP_SUPPORT_PJPG)
```
#### Output log
```c
    [x/xx] Move and Pack assets...
    --support_format: ['.png']
    --support_spng: False
    --support_sjpg: False
    --support_qoi: False
    --support_pjpg: True
    --support_raw: False
    Completed example.png -> example.pjpg
```

### Initialization
```c
    esp_lv_decoder_handle_t decoder_handle = NULL;
    esp_lv_decoder_init(&decoder_handle); //Initialize this after lvgl starts
```

Notes:
- PJPG is produced by packing two baseline JPEG streams (RGB and Alpha) behind a small custom header. The tool automatically invokes an internal converter (`png_processor.py`).
- A guard may skip PJPG for very small images to avoid overhead (default threshold is 128x128). You will see a log like: "Skip PJPG for small image ...".

### Hardware JPEG / PJPG: Usage Notes and Limitations

- General
  - Hardware acceleration is used only when available and enabled. If unavailable, software decoding is used transparently.
  - File and in-memory (C array) sources are both supported.

- Standard JPEG (JPG/JPEG)
  - Hardware path is taken only when both width and height are multiples of 16 and at least 64 pixels. Otherwise, software decoding is used.
  - Output color format follows the default display: RGB565 or RGB888.
  - For best compatibility, use baseline (non‑progressive) JPEG. Progressive JPEG may not be supported by the hardware decoder and can fall back to software or fail depending on ESP-IDF version.

- PJPG (PNG with Alpha -> PJPG)
  - Output color format is `LV_COLOR_FORMAT_RGB565A8` (packed RGB565 + 8‑bit Alpha plane).
  - For best performance, use images with width and height aligned to 16 pixels. If not aligned, the decoder internally decodes into an aligned buffer and copies/crops back, which adds extra RAM usage and time.
  - The Alpha plane is decoded as an 8‑bit grayscale JPEG stream.

- Split formats (SJPG/SPNG/SQOI)
  - These split containers are decoded slice‑by‑slice for low RAM usage, so LVGL zoom/rotate are not supported. PJPG is not a split format; LVGL can post‑process the fully decoded buffer as usual (subject to LVGL performance/memory constraints).

- Memory and alignment details
  - Decoder output buffers are allocated with 128‑byte alignment to cooperate with caches and, when present, the hardware JPEG engine. Unaligned image sizes may require temporary aligned buffers during hardware decoding.

