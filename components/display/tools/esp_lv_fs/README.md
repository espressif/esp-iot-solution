[![Component Registry](https://components.espressif.com/components/espressif/esp_lv_fs/badge.svg)](https://components.espressif.com/components/espressif/esp_lv_fs)

## Instructions and Details

`esp_lv_fs` allows LVGL to use filesystems for accessing assets. This component integrates with `esp_mmap_assets` to efficiently manage file operations and supports multiple partitions.

### Features
    - Integrates with `esp_mmap_assets` for filesystem creation.

    - Supports standard file operations: fopen, fclose, fread, ftell, and fseek.

    - Uses the `esp_partition_read` API for efficient file access.

    - Supports multiple partitions.

## Add to project

Packages from this repository are uploaded to [Espressif's component service](https://components.espressif.com/).
You can add them to your project via `idf.py add-dependancy`, e.g.
```
    idf.py add-dependency esp_lv_fs
```

## Usage

### Dependencies
The [esp_mmap_assets](https://components.espressif.com/components/espressif/esp_mmap_assets) component is required. It provides file offset index relationship.

### Initialization
```c
    #include "esp_lv_fs.h"
    #include "esp_mmap_assets.h"

    esp_lv_fs_handle_t fs_drive_a_handle;
    mmap_assets_handle_t mmap_drive_a_handle;

    const mmap_assets_config_t asset_cfg = {
        .partition_label = "assets_A",
        .max_files = MMAP_DRIVE_A_FILES,
        .checksum = MMAP_DRIVE_A_CHECKSUM,
        .flags = {
            .mmap_enable = true,
        }
    };
    mmap_assets_new(&asset_cfg, &mmap_drive_a_handle);
 
    const fs_cfg_t fs_drive_a_cfg = {
        .fs_letter = 'A',
        .fs_assets = mmap_drive_a_handle,
        .fs_nums = MMAP_DRIVE_A_FILES
    };
    esp_lv_fs_desc_init(&fs_drive_a_cfg, &fs_drive_a_handle); //Initialize this after lvgl starts
```
