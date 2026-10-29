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

    - Any development board with a USB OTG interface capable of providing external power.
    - A USB flash drive using the BOT (Bulk-Only Transport) protocol and Transparent SCSI command set.

Partition Table:

    - Includes an OTA partition.

Code examples
---------------

1. Call `esp_msc_host_install` to initialize the MSC host driver.
   Set ``skip_init_usb_host_driver`` to true only when the application has already installed the USB Host Library and has a task calling ``usb_host_lib_handle_events()``.

.. code:: c

    esp_msc_host_config_t msc_host_config = {
        .base_path = "/usb",
        .host_driver_config = DEFAULT_MSC_HOST_DRIVER_CONFIG(),
        .vfs_fat_mount_config = DEFAULT_ESP_VFS_FAT_MOUNT_CONFIG(),
        .host_config = DEFAULT_USB_HOST_CONFIG()
    };
    esp_msc_host_handle_t host_handle = NULL;
    esp_msc_host_install(&msc_host_config, &host_handle);

2. Call `esp_msc_ota` to complete OTA updates. `host_handle` is required so OTA can wait for the MSC VFS mount and keep it locked while reading. Use :cpp:type:`ota_bin_path` to specify the OTA file path and :cpp:type:`wait_msc_connect` to specify the waiting time for USB drive insertion in FreeRTOS ticks.

.. code:: c

    esp_msc_ota_config_t config = {
        .host_handle = host_handle,
        .ota_bin_path = "/usb/ota_test.bin",
        .wait_msc_connect = pdMS_TO_TICKS(5000),
    };
    esp_msc_ota(&config);

3. Call ``esp_restart()`` after a successful OTA update. If the application needs to shut down the MSC host instead, call ``esp_msc_host_uninstall()`` only after the USB disk is disconnected and the MSC device is no longer mounted.

4. Call `esp_event_handler_register` to register the event handler for obtaining OTA process details.

.. code:: c

    esp_event_loop_create_default();
    esp_event_handler_register(ESP_MSC_OTA_EVENT, ESP_EVENT_ANY_ID, &event_handler, NULL);

5. To observe MSC host events (device connect/disconnect, device install/uninstall, VFS register/unregister), set :cpp:member:`event_cb` and :cpp:member:`event_cb_arg` in :cpp:type:`esp_msc_host_config_t`. Starting from ``v2.0.0``, the MSC host no longer dispatches events through the default ``esp_event`` loop and the ``ESP_MSC_HOST_EVENT`` event base has been removed; this callback is now the only notification channel for host events.

.. code:: c

    static void msc_host_event_cb(esp_msc_host_handle_t handle,
                                  esp_msc_host_event_t event, void *user_ctx)
    {
        switch (event) {
        case ESP_MSC_HOST_CONNECT:
        case ESP_MSC_HOST_DISCONNECT:
        case ESP_MSC_HOST_DEVICE_INSTALL:
        case ESP_MSC_HOST_DEVICE_UNINSTALL:
        case ESP_MSC_HOST_VFS_REGISTER:
        case ESP_MSC_HOST_VFS_UNREGISTER:
            // Handle the corresponding event
            break;
        }
    }

    esp_msc_host_config_t msc_host_config = {
        /* ... */
        .event_cb = msc_host_event_cb,
        .event_cb_arg = NULL,
    };

API Reference
----------------

.. include-build-file:: inc/esp_msc_host.inc

.. include-build-file:: inc/esp_msc_ota.inc
