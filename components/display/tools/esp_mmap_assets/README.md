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


## Add to project

Packages from this repository are uploaded to [Espressif's component service](https://components.espressif.com/).
You can add them to your project via `idf.py add-dependancy`, e.g.
```
    idf.py add-dependency esp_mmap_assets
```

Alternatively, you can create `idf_component.yml`. More is in [Espressif's documentation](https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-guides/tools/idf-component-manager.html).

## Usage

### CMake
Optionally, users can opt to have the image automatically flashed together with the app binaries, partition tables, etc. on idf.py flash by specifying FLASH_IN_PROJECT. For example:
```c
    spiffs_create_partition_assets(
        my_spiffs_partition
        my_folder
        FLASH_IN_PROJECT
        MMAP_FILE_SUPPORT_FORMAT ".png"
        MMAP_SUPPORT_QOI
        MMAP_SUPPORT_SQOI
        MMAP_SPLIT_HEIGHT 16)
```
Additionally, the following options are supported. These options allow you to configure various aspects of image handling, such as enabling support for different formats (SJPG, SPNG, QOI, SQOI, RAW), and controlling how RAW images are processed.
```c
    set(options FLASH_IN_PROJECT,          // Defines storage type (flash in project)
                MMAP_SUPPORT_SJPG,         // Enable support for SJPG format
                MMAP_SUPPORT_SPNG,         // Enable support for SPNG format
                MMAP_SUPPORT_QOI,          // Enable support for QOI format
                MMAP_SUPPORT_SQOI,         // Enable support for SQOI format
                MMAP_SUPPORT_RAW,          // Enable support for RAW format (LVGL conversion only)
                MMAP_RAW_DITHER,           // Enable dithering for RAW images (LVGL conversion only)
                MMAP_RAW_BGR_MODE)         // Enable BGR mode for RAW images (LVGL conversion only)

    set(one_value_args MMAP_FILE_SUPPORT_FORMAT,    // Specify supported file format (e.g., .png, .jpg)
                   MMAP_SPLIT_HEIGHT,               // Define the height for image splitting
                   MMAP_RAW_FILE_FORMAT)            // Specify the file format for RAW images (LVGL conversion only)

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
