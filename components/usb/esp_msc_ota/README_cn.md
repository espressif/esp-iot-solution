[![Component Registry](https://components.espressif.com/components/espressif/esp_msc_ota/badge.svg)](https://components.espressif.com/components/espressif/esp_msc_ota)

## ESP MSC OTA 组件介绍

``esp_msc_ota`` 是基于 ESP32 USB OTG 外设的 U 盘 OTA 程序。

* 支持 U 盘热插拔
* 支持 U 盘 OTA 功能。

### 用户指南

请查看: https://docs.espressif.com/projects/esp-iot-solution/zh_CN/latest/usb/usb_host/esp_msc_ota.html

### 添加组件到工程

请使用组件管理器指令 `add-dependency` 将 `esp_msc_ota` 添加到项目的依赖项, 在 `CMake` 执行期间该组件将被自动下载到工程目录。

    ```
    idf.py add-dependency "espressif/esp_msc_ota=*"
    ```

## 使用示例

```C
    esp_event_loop_create_default();
    ESP_ERROR_CHECK(esp_event_handler_register(ESP_MSC_OTA_EVENT, ESP_EVENT_ANY_ID, &msc_ota_event_handler, NULL));
    esp_msc_host_config_t msc_host_config = {
        .base_path = "/usb",
        .host_driver_config = DEFAULT_MSC_HOST_DRIVER_CONFIG(),
        .vfs_fat_mount_config = DEFAULT_ESP_VFS_FAT_MOUNT_CONFIG(),
        .host_config = DEFAULT_USB_HOST_CONFIG()
    };
    esp_msc_host_handle_t host_handle = NULL;
    esp_msc_host_install(&msc_host_config, &host_handle);
    esp_msc_ota_config_t config = {
        .ota_bin_path = "/usb/ota_test.bin",
        .wait_msc_connect = pdMS_TO_TICKS(5000),
    };
    esp_msc_ota(&config);
    esp_msc_host_uninstall(host_handle);
```

## 注意事项

* 默认的文件系统，文件名不要超过 11 个字，如果需要支持长命名文件。请打开下方的任意宏

    * CONFIG_FATFS_LFN_HEAP
    * CONFIG_FATFS_LFN_STACK
