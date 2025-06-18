[![Component Registry](https://components.espressif.com/components/espressif/esp_mmap_assets/badge.svg)](https://components.espressif.com/components/espressif/esp_mmap_assets)

## Instructions and Details

This module is primarily used for packaging assets (such as images, fonts, etc.) and directly mapping them for user access.

### Features

1. **Separation of Code and Assets**:
    - Assets are kept separate from the application code, reducing the size of the application binary and improving performance compared to using SPIFFS.

2. **Efficient Resource Management**:
    - Simplifies assets management by using automatically generated enums to access resource information.

3. **Memory-Efficient Image Decoding**:
    - Includes an image splitting script to reduce the memory required for image decoding.

4. **Support for Image Conversion (JPG/PNG to QOI)**:
    - Supports image conversion from JPG and PNG formats to QOI (Quite OK Image), enabling faster decoding times for resource-limited systems.

5. **LVGL-Specific Binary Format Support**:
    - Supports image conversion to binary files required by LVGL, ensuring compatibility with the LVGL graphics library and optimizing performance for display rendering.

## Add to Project

Packages from this repository are uploaded to [Espressif's component service](https://components.espressif.com/). You can add them to your project via `idf.py add-dependency`, e.g.

```sh
idf.py add-dependency esp_mmap_assets
```

Alternatively, you can create `idf_component.yml`. More details are available in [Espressif's documentation](https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-guides/tools/idf-component-manager.html).

## Usage

### CMake

The following options are supported. These options allow you to configure various aspects of image handling.

```c
set(options
    FLASH_IN_PROJECT,
    FLASH_APPEND_APP,
    MMAP_SUPPORT_SJPG,
    MMAP_SUPPORT_SPNG,
    MMAP_SUPPORT_QOI,
    MMAP_SUPPORT_SQOI,
    MMAP_SUPPORT_RAW,
    MMAP_RAW_DITHER,
    MMAP_RAW_BGR_MODE)
```

```c
set(one_value_args
    MMAP_FILE_SUPPORT_FORMAT,
    MMAP_SPLIT_HEIGHT,
    MMAP_RAW_FILE_FORMAT
    IMPORT_INC_PATH)
```

### Option Explanations

#### General Options

- **`FLASH_IN_PROJECT`**: Users can opt to have the image automatically flashed together with the app binaries, partition tables, etc. on `idf.py flash`
- **`FLASH_APPEND_APP`**: Enables appending binary data (`bin`) to the application binary (`app_bin`).
- **`IMPORT_INC_PATH`**: Target path for generated include files. Defaults to referencing component location.
- **`MMAP_FILE_SUPPORT_FORMAT`**: Specifies supported file formats (e.g., `.png`, `.jpg`, `.ttf`).
- **`MMAP_SPLIT_HEIGHT`**: Defines height for image splitting to reduce memory usage. Depends on:
  - `MMAP_SUPPORT_SJPG`
  - `MMAP_SUPPORT_SPNG`
  - `MMAP_SUPPORT_SQOI`

  ##### General demo

    ```c
    spiffs_create_partition_assets(
        my_spiffs_partition
        my_folder
        FLASH_IN_PROJECT
        MMAP_FILE_SUPPORT_FORMAT ".jpg,.png,.ttf"
   )
    ```

#### Supported Image Formats

- **`MMAP_SUPPORT_SJPG`**: Enables support for **SJPG** format.
- **`MMAP_SUPPORT_SPNG`**: Enables support for **SPNG** format.
- **`MMAP_SUPPORT_QOI`**: Enables support for **QOI** format.
- **`MMAP_SUPPORT_SQOI`**: Enables support for **SQOI** format. Depends on:
  - `MMAP_SUPPORT_QOI`

  ##### Image Splitting Demo

    ```c
    spiffs_create_partition_assets(
        my_spiffs_partition
        my_folder
        FLASH_IN_PROJECT
        MMAP_FILE_SUPPORT_FORMAT ".jpg"
        MMAP_SUPPORT_SJPG
        MMAP_SPLIT_HEIGHT 16
   )
    ```

#### LVGL Bin Support

- **`MMAP_SUPPORT_RAW`**: Converts images to LVGL-supported **Binary** data.

    **References:**
    - LVGL v8: [Use detailed reference](https://github.com/W-Mai/lvgl_image_converter)
    - LVGL v9: [Use detailed reference](https://github.com/lvgl/lvgl/blob/master/scripts/LVGLImage.py)

- **`MMAP_RAW_FILE_FORMAT`**: Specifies file format for **RAW** images.
    - **LVGL v8**: `{true_color, true_color_alpha, true_color_chroma, indexed_1, indexed_2, indexed_4, indexed_8, alpha_1, alpha_2, alpha_4, alpha_8, raw, raw_alpha, raw_chroma}`
    - **LVGL v9**: Not used.

- **`MMAP_RAW_COLOR_FORMAT`**: Specifies color format for **RAW** images.
    - **LVGL v8**: `{RGB332, RGB565, RGB565SWAP, RGB888}`
    - **LVGL v9**: `{L8, I1, I2, I4, I8, A1, A2, A4, A8, ARGB8888, XRGB8888, RGB565, RGB565A8, ARGB8565, RGB888, AUTO, RAW, RAW_ALPHA}`

- **`MMAP_RAW_DITHER`**: Enables **dithering** for **RAW** images.
    - **LVGL v8**: Requires dithering.
    - **LVGL v9**: Not used.

- **`MMAP_RAW_BGR_MODE`**: Enables **BGR mode** for **RAW** images.
    - **LVGL v8**: Not used.
    - **LVGL v9**: Not used.

    ##### LVGL v9 demo

    ```c
    spiffs_create_partition_assets(
            .........
            MMAP_FILE_SUPPORT_FORMAT ".png"
            MMAP_SUPPORT_RAW
            MMAP_RAW_COLOR_FORMAT "ARGB8888"
    )
    ```

    ##### LVGL v8 demo

    ```c
    spiffs_create_partition_assets(
            .........
            MMAP_FILE_SUPPORT_FORMAT ".png"
            MMAP_SUPPORT_RAW
            MMAP_RAW_FILE_FORMAT "true_color_alpha"
            MMAP_RAW_COLOR_FORMAT "RGB565SWAP"
    )
    ```

### Initialization
```c
    mmap_assets_handle_t asset_handle;

    /* partitions.csv
    * --------------------------------------------------------
    * | Name                | Type | SubType | Offset | Size  | Flags     |
    * --------------------------------------------------------
    * | my_spiffs_partition | data | spiffs  |        | 6000K |           |
    * --------------------------------------------------------
    */
    const mmap_assets_config_t config = {
        .partition_label = "my_spiffs_partition",
        .max_files = MMAP_MY_FOLDER_FILES, //Get it from the compiled .h
        .checksum = MMAP_MY_FOLDER_CHECKSUM, //Get it from the compiled .h
        .flags = {
            .mmap_enable = true,
            .app_bin_check = true,
        },
    };

    ESP_ERROR_CHECK(mmap_assets_new(&config, &asset_handle));

    const char *name = mmap_assets_get_name(asset_handle, 0);
    const void *mem = mmap_assets_get_mem(asset_handle, 0);
    int size = mmap_assets_get_size(asset_handle, 0);
    int width = mmap_assets_get_width(asset_handle, 0);
    int height = mmap_assets_get_height(asset_handle, 0);

    ESP_LOGI(TAG, "Asset - Name:[%s], Memory:[%p], Size:[%d bytes], Width:[%d px], Height:[%d px]", name, mem, size, width, height);

```
