ESP MSC OTA
==============

:link_to_translation:`en:[English]`

`esp_msc_ota` 是基于 USB MSC 的 OTA 驱动程序，它支持通过 USB 从 U 盘中读取程序，烧录到指定 OTA 分区，从而实现 OTA 升级的功能。

特性：

1. 支持通过 USB 接口读取 U 盘，并实现 OTA 升级
2. 支持 U 盘热插拔

用户指南
---------

硬件需求：

    - 任何带有 USB 接口的 ESP32-S2/ESP32-S3/ESP32-P4 开发板, 且 USB 接口需要能够向外供电
    - 使用 BOT（仅限大容量传输）协议和 Transparent SCSI 命令集的 U 盘。

分区表：

    - 具有 OTA 分区

代码示例
-------------

1. 调用 `esp_msc_host_install` 初始化 MSC 主机驱动程序

.. code:: c

    esp_msc_host_config_t msc_host_config = {
        .base_path = "/usb",
        .host_driver_config = DEFAULT_MSC_HOST_DRIVER_CONFIG(),
        .vfs_fat_mount_config = DEFAULT_ESP_VFS_FAT_MOUNT_CONFIG(),
        .host_config = DEFAULT_USB_HOST_CONFIG()
    };
    esp_msc_host_handle_t host_handle = NULL;
    esp_msc_host_install(&msc_host_config, &host_handle);

2. 调用 `esp_msc_ota` 完成 OTA 升级。通过 :cpp:type:`ota_bin_path` 指定 OTA 文件路径，通过 :cpp:type:`wait_msc_connect` 指定等待 U 盘插入的时间，单位为 freertos tick。

.. code:: c

    esp_msc_ota_config_t config = {
        .ota_bin_path = "/usb/ota_test.bin",
        .wait_msc_connect = pdMS_TO_TICKS(5000),
    };
    esp_msc_ota(&config);

3. 调用 `esp_event_handler_register` 注册事件处理程序，获取 ota 过程细节。

.. code:: c

    esp_event_loop_create_default();
    esp_event_handler_register(ESP_MSC_OTA_EVENT, ESP_EVENT_ANY_ID, &event_handler, NULL);

API Reference
--------------

.. include-build-file:: inc/esp_msc_ota.inc
