# RAMFS

## Description

RAMFS is a RAM-backed filesystem component for ESP-IDF. It registers a mount point with ESP-IDF VFS, so applications can use standard POSIX and C file APIs such as `open()`, `read()`, `write()`, `mkdir()`, `rename()`, `stat()`, `fopen()`, and `fread()` on files stored in RAM.

This component is useful for temporary files, staging data before writing it to flash or SD card, test fixtures, and workloads that need fast file-like access without flash wear.

Main features:

* Mount one or more independent RAMFS instances through VFS.
* Limit file content capacity with `max_bytes`.
* Limit simultaneously open files with `max_files`.
* Select allocation memory by heap capability flags, for example internal RAM or PSRAM.
* Support common file operations including read, write, append, seek, pread, pwrite, truncate, ftruncate, stat, access, utime, mkdir, rmdir, readdir, rename, and unlink.
* Support loading files or directory trees from an external filesystem path and syncing RAMFS contents back to an external filesystem path.

## Add Component to Your Project

If this component is used from the ESP Component Registry, add it to your project with:

```shell
idf.py add-dependency "espressif/ramfs=*"
```

If this component is used directly from `esp-iot-solution`, add `components/storage/ramfs` to your project component search path.

## How to Use

Include `ramfs.h`, register a mount point, then access files under that mount point with normal filesystem APIs.

```c
#include <fcntl.h>
#include <unistd.h>

#include "ramfs.h"
#include "esp_err.h"
#include "esp_heap_caps.h"

void app_main(void)
{
    const ramfs_config_t config = {
        .base_path = "/ram",
        .max_files = 8,
        .max_bytes = 64 * 1024,
        .caps = MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT,
    };

    ESP_ERROR_CHECK(ramfs_register(&config));

    int fd = open("/ram/hello.txt", O_CREAT | O_WRONLY | O_TRUNC, 0666);
    write(fd, "hello ramfs", 11);
    close(fd);

    ESP_ERROR_CHECK(ramfs_unregister("/ram"));
}
```

To allocate file data and metadata from PSRAM, use a capability set that includes `MALLOC_CAP_8BIT`, for example:

```c
.caps = MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT,
```

## Capacity Information

Use `ramfs_info()` to query the configured file-content capacity and current file-content usage.

```c
size_t total_bytes = 0;
size_t used_bytes = 0;

ESP_ERROR_CHECK(ramfs_info("/ram", &total_bytes, &used_bytes));
```

`max_bytes` only limits file contents. Directory metadata, file nodes, names, mount context, and open-file state are allocated from the selected heap capabilities but are not counted in `used_bytes`.

## Sync with External Filesystem

RAMFS contents are volatile. Use the sync/load helpers when data needs to be copied between RAMFS and another mounted filesystem such as FATFS on flash or SD card.

```c
ESP_ERROR_CHECK(ramfs_sync_file_to_fatfs("/ram/config.json", "/fatfs/config.json"));
ESP_ERROR_CHECK(ramfs_sync_tree_to_fatfs("/ram/session", "/fatfs/session"));

ESP_ERROR_CHECK(ramfs_load_file_from_fatfs("/fatfs/config.json", "/ram/config.json"));
ESP_ERROR_CHECK(ramfs_load_tree_from_fatfs("/fatfs/session", "/ram/session"));
```

The external filesystem must be initialized and mounted by the application before these helpers are called. The helpers reject busy RAMFS nodes that are still open.

## Notes

* `base_path` must be an absolute VFS mount path, such as `/ram`. It must not be `/` and must not end with `/`.
* `max_files` controls the number of simultaneously open files, not the number of files stored in RAMFS.
* `caps` must include `MALLOC_CAP_8BIT`.
* Directory APIs require `CONFIG_VFS_SUPPORT_DIR` to be enabled.
* Data is stored only in RAM and is lost after reboot, unregister, or power loss unless it is explicitly synced to persistent storage.