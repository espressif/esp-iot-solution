[![Component Registry](https://components.espressif.com/components/espressif/esp_msc_ota/badge.svg)](https://components.espressif.com/components/espressif/esp_msc_ota)

## Introduction to ESP MSC OTA Component

``esp_msc_ota`` is a USB disk OTA program based on ESP32 USB OTG peripheral.

* Supports hot-plugging of USB disk.
* Supports USB disk OTA functionality.

### User Guide

Please refer: https://docs.espressif.com/projects/esp-iot-solution/en/latest/usb/usb_host/esp_msc_ota.html

### Adding the Component to the Project

Please use the component manager command `add-dependency` to add `esp_msc_ota` as a dependency to your project. During the `CMake` execution, this component will be automatically downloaded to the project directory.

    ```
    idf.py add-dependency "espressif/esp_msc_ota=*"
    ```

## Usage Example

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

## Notes

* For the default file system, filenames should not exceed 11 characters. If support for long-named files is needed, please enable any of the macros below:

    * CONFIG_FATFS_LFN_HEAP
    * CONFIG_FATFS_LFN_STACK