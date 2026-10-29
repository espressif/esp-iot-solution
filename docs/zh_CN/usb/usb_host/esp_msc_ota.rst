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

    - 任何带有 USB OTG 接口的开发板, 且 USB 接口需要能够向外供电
    - 使用 BOT（仅限大容量传输）协议和 Transparent SCSI 命令集的 U 盘。

分区表：

    - 具有 OTA 分区

代码示例
-------------

1. 调用 `esp_msc_host_install` 初始化 MSC 主机驱动程序
   仅当应用已经安装 USB Host Library，并且已有任务调用 ``usb_host_lib_handle_events()`` 时，才将 ``skip_init_usb_host_driver`` 设置为 true。

.. code:: c

    esp_msc_host_config_t msc_host_config = {
        .base_path = "/usb",
        .host_driver_config = DEFAULT_MSC_HOST_DRIVER_CONFIG(),
        .vfs_fat_mount_config = DEFAULT_ESP_VFS_FAT_MOUNT_CONFIG(),
        .host_config = DEFAULT_USB_HOST_CONFIG()
    };
    esp_msc_host_handle_t host_handle = NULL;
    esp_msc_host_install(&msc_host_config, &host_handle);

2. 调用 `esp_msc_ota` 完成 OTA 升级。`host_handle` 为必填项，OTA 会通过它等待 MSC VFS 挂载，并在读取文件期间保持锁定。通过 :cpp:type:`ota_bin_path` 指定 OTA 文件路径，通过 :cpp:type:`wait_msc_connect` 指定等待 U 盘插入的时间，单位为 freertos tick。

.. code:: c

    esp_msc_ota_config_t config = {
        .host_handle = host_handle,
        .ota_bin_path = "/usb/ota_test.bin",
        .wait_msc_connect = pdMS_TO_TICKS(5000),
    };
    esp_msc_ota(&config);

3. OTA 成功后调用 ``esp_restart()`` 重启。如果应用需要关闭 MSC host，则仅当 U 盘已拔出且 MSC 设备不再挂载时，才调用 ``esp_msc_host_uninstall()``。

4. 调用 `esp_event_handler_register` 注册事件处理程序，获取 ota 过程细节。

.. code:: c

    esp_event_loop_create_default();
    esp_event_handler_register(ESP_MSC_OTA_EVENT, ESP_EVENT_ANY_ID, &event_handler, NULL);

5. 如需感知 MSC host 端事件（U 盘连接/断开、设备安装/卸载、VFS 注册/注销等），请在 :cpp:type:`esp_msc_host_config_t` 中设置 :cpp:member:`event_cb` 与 :cpp:member:`event_cb_arg`。从 ``v2.0.0`` 起，MSC host 不再通过默认 ``esp_event`` 循环派发事件，也不再提供 ``ESP_MSC_HOST_EVENT`` event base，只能通过该回调获取通知。

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
            // 处理对应事件
            break;
        }
    }

    esp_msc_host_config_t msc_host_config = {
        /* ... */
        .event_cb = msc_host_event_cb,
        .event_cb_arg = NULL,
    };

API Reference
--------------

.. include-build-file:: inc/esp_msc_host.inc

.. include-build-file:: inc/esp_msc_ota.inc
