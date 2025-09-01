ESP MSC OTA
==============

:link_to_translation:`zh_CN:[中文]`

`esp_msc_ota`` is an OTA (Over-The-Air) driver based on USB MSC (USB Mass Storage Class). It supports reading programs from a USB flash drive and burning them into a designated OTA partition, thereby enabling OTA upgrades via USB.

Features:

1. Supports OTA updates by retrieving programs from a USB flash drive via USB interface.
2. Supports hot-plugging of the USB flash drive.

User Guide
------------

Hardware requirements:

    - Any ESP32-S2/ESP32-S3/ESP32-P4 development board with a USB interface capable of providing external power.
    - A USB flash drive using the BOT (Bulk-Only Transport) protocol and Transparent SCSI command set.

Partition Table:

    - Includes an OTA partition.

Code examples
---------------

1. Call `esp_msc_host_install` to initialize the MSC host driver.

.. code:: c

    esp_msc_host_config_t msc_host_config = {
        .base_path = "/usb",
        .host_driver_config = DEFAULT_MSC_HOST_DRIVER_CONFIG(),
        .vfs_fat_mount_config = DEFAULT_ESP_VFS_FAT_MOUNT_CONFIG(),
        .host_config = DEFAULT_USB_HOST_CONFIG()
    };
    esp_msc_host_handle_t host_handle = NULL;
    esp_msc_host_install(&msc_host_config, &host_handle);

2. Call `esp_msc_ota` to complete OTA updates. Use :cpp:type:`ota_bin_path` to specify the OTA file path and :cpp:type:`wait_msc_connect` to specify the waiting time for USB drive insertion in FreeRTOS ticks.

.. code:: c

    esp_msc_ota_config_t config = {
        .ota_bin_path = "/usb/ota_test.bin",
        .wait_msc_connect = pdMS_TO_TICKS(5000),
    };
    esp_msc_ota(&config);

3. Call `esp_event_handler_register` to register the event handler for obtaining OTA process details.

.. code:: c

    esp_event_loop_create_default();
    esp_event_handler_register(ESP_MSC_OTA_EVENT, ESP_EVENT_ANY_ID, &event_handler, NULL);

API Reference
----------------

.. include-build-file:: inc/esp_msc_ota.inc
