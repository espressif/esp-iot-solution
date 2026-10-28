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

Requires ESP-IDF >= 5.3 and LVGL >= 8.3, < 10.

Packages from this repository are uploaded to [Espressif's component service](https://components.espressif.com/).
Add the component with:
```
    idf.py add-dependency esp_lv_decoder
```

## Usage
For build-time image conversion and packaging, also add [esp_mmap_assets](https://components.espressif.com/components/espressif/esp_mmap_assets):

```sh
idf.py add-dependency esp_mmap_assets
```

### Converting JPG to SJPG
#### CMake
```cmake
    spiffs_create_partition_assets(
        my_spiffs_partition
        my_folder
        FLASH_IN_PROJECT
        MMAP_FILE_SUPPORT_FORMAT ".jpg"
        MMAP_SUPPORT_SJPG
        MMAP_SPLIT_HEIGHT 16)
```
#### Output log
```text
Completed navi_52.jpg -> navi_52.sjpg
```

### Converting PNG to SPNG
#### CMake
```cmake
    spiffs_create_partition_assets(
        my_spiffs_partition
        my_folder
        FLASH_IN_PROJECT
        MMAP_FILE_SUPPORT_FORMAT ".png"
        MMAP_SUPPORT_SPNG
        MMAP_SPLIT_HEIGHT 16)
```
#### Output log
```text
Completed navi_52.png -> navi_52.spng
```

### Converting PNG、JPG to QOI
#### CMake
```cmake
    spiffs_create_partition_assets(
        my_spiffs_partition
        my_folder
        FLASH_IN_PROJECT
        MMAP_FILE_SUPPORT_FORMAT ".jpg,.png"
        MMAP_SUPPORT_QOI)
```
#### Output log
```text
Completed navi_52.png -> navi_52.qoi
Completed navi_53.jpg -> navi_53.qoi
```

### Converting PNG、JPG to SQOI
#### CMake
```cmake
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
```text
Completed navi_52.png -> navi_52.sqoi
Completed navi_53.jpg -> navi_53.sqoi
```

### Converting PNG to PJPG
#### CMake
```cmake
    spiffs_create_partition_assets(
        my_spiffs_partition
        my_folder
        FLASH_IN_PROJECT
        MMAP_FILE_SUPPORT_FORMAT ".png"
        MMAP_SUPPORT_PJPG)
```

PJPG stores color and alpha as lossy JPEG streams. Converted dimensions are padded to multiples of 16. Images with either side below 128 pixels or complex alpha may remain PNG.

### Initialization
```c
    esp_lv_decoder_handle_t decoder_handle = NULL;
    esp_lv_decoder_init(&decoder_handle); // After LVGL initialization
```

When using `esp_lvgl_adapter` with `CONFIG_ESP_LVGL_ADAPTER_ENABLE_DECODER=y`, registration is automatic. Display images through a mounted LVGL filesystem or an in-memory image descriptor.

### JPEG / PJPG Notes

- General
  - When hardware JPEG is unavailable or disabled, software decoding uses `esp_new_jpeg` on targets supported by both components.
  - Only baseline (non-progressive) JPEG is supported.

- Standard JPEG (JPG/JPEG)
  - Hardware path is taken only when both width and height are multiples of 16 and at least 64 pixels. Otherwise, software decoding is used.
  - Output color format follows the default display: RGB565 or RGB888.

- PJPG (PNG with Alpha -> PJPG)
  - Enabled by default; define `ESP_LV_ENABLE_PJPG=0` to disable.
  - Define `ESP_LV_ENABLE_HW_JPEG=0` to force software decoding. PJPG hardware errors are returned without a software retry.
  - Default output: RGB565A8. LVGL v8 requires `LV_COLOR_DEPTH=16`; LVGL v9 also supports the ARGB8888 output option.
  - RGB565A8 needs approximately `width * height * 3` bytes for the decoded image, plus working buffers. Large images may require PSRAM.

- Split formats (SJPG/SPNG/SQOI)
  - Decoded in slices to reduce RAM usage; zoom and rotation are unsupported. PJPG uses a full decoded image.
