# LOG ROUTER

The `log_router` is an extension component of `esp_log` that allows users to save logs of specific levels and tags through the file system. By configuring the save path, log level, and tag filter, logs can be redirected to corresponding log files while preserving the original log output. This component provides the following features:

1. Supports redirecting logs of specific levels to the file system while preserving the original log output interface
2. Supports filtering logs by specific tags to capture only relevant log messages
3. Supports batch writing of logs to the file system to avoid frequent flash operations

## How to Configure Router Parameters

First, you need to define a log partition with type `data` and subtype `fat` in the partition table, or access an SD card or external flash through the FAT file system, and complete the initialization and mounting of the FAT file system in your application.

This mounting process is typically implemented using convenience functions such as `esp_vfs_fat_spiflash_mount()` or `esp_vfs_fat_sdmmc_mount()`, which are responsible for formatting the underlying FAT partition (if needed), mounting it, and registering it to ESP-IDF's Virtual File System (VFS). Once completed, you can use standard C file operation functions (such as fopen(), fread(), fprintf(), etc.) to read and write to this file system, just like operating on local files, facilitating tasks such as log storage and configuration persistence.

> **Note:** `log_router` as a middleware does not handle the initialization of the FAT file system. Please complete the initialization and mounting of the FAT file system before using `log_router`. Additionally, the current version may occupy certain system time when writing due to timeout or reaching the set threshold, which may affect some time-sensitive programs.

## Add component to your project

Please use the component manager command `add-dependency` to add the `log_router` to your project's dependency, during the `CMake` step the component will be downloaded automatically

```
idf.py add-dependency "espressif/log_router=*"
```

Alternatively, you can create `idf_component.yml`. More is in [Espressif's documentation](https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-guides/tools/idf-component-manager.html).

## How to use

The usage of ``log_router`` is very simple. You only need to call `esp_log_router_to_file` and fill in the file to redirect to, the specified `tag` and `level` to achieve log redirection. Note that if `tag` is `NULL`, all logs greater than or equal to the set `level` will be saved.

```c
// Step1: Initialize FAT file system

...

// Step2: Redirect log output

esp_log_router_to_file("/log/log.txt", TAG, ESP_LOG_WARN);

// Step3: If you want to cancel log saving

esp_log_delete_router("/log/log.txt");

```

If you are using ``fatfs``, you can use the ``fatfsparse.py`` provided in the ``ESP-IDF`` SDK to parse it out. Take the following partition table as an example:

```csv
# Name,     Type, SubType, Offset,   Size, Flags
# Note: if you have increased the bootloader size, make sure to update the offsets to avoid overlap
factory,  app,  factory, 0x10000, 1M,
log,      data, fat,       ,  500K,
```

First, you need to read out the content of the specific partition:

```shell
parttool.py -p /dev/ttyUSB0 read_partition --partition-name=log --output "log.bin"
```

Next, use the parsing script ``fatfsparse.py`` for parsing, which is located in the ``components/fatfs`` directory under the SDK:

```shell
cd ${IDF_PATH}/components/fatfs
./fatfsparse.py log.bin
```
