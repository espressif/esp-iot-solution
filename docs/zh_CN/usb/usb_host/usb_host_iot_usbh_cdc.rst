USB 主机 CDC
=====================

:link_to_translation:`en:[English]`

``iot_usbh_cdc`` 组件实现了一个 USB 主机 CDC 驱动的简化版本。该 API 的设计类似于 `ESP-IDF UART 驱动程序 <https://docs.espressif.com/projects/esp-idf/en/latest/esp32s3/api-reference/peripherals/uart.html>`_，可在一些应用中替代 UART，快速实现从 UART 到 USB 的迁移。

使用指南
---------------

1. 使用 ``usbh_cdc_install`` 配置, 用户可以配置好 USB CDC 驱动程序，并通过设置 ``skip_init_usb_host_driver`` 配置项，在组件内部初始化好 USB HOST Driver 协议栈。

.. code:: c

    /* 安装 USB CDC 驱动，并由驱动内部初始化好 USB Host Driver 协议栈 */
    usbh_cdc_driver_config_t config = {
        .driver_task_stack_size = 1024 * 4,
        .driver_task_priority = 5,
        .xCoreID = 0,
        .skip_init_usb_host_driver = false,
        .new_dev_cb = cdc_new_dev_cb,
    };

2. 使用 ``usbh_cdc_create`` 配置，用户可以简单配置 itf_num 接口号和内部 ringbuffer 的大小，除此之外，用户还可以配置热插拔相关的回调函数 ``connect`` ``disconnect`` ``revc_data``：

.. code:: c

    /* 安装 USB 主机 CDC 驱动程序，配置 bulk 端点地址和内部 ringbuffer 的大小 */
    usbh_cdc_device_config_t dev_config = {
        .vid = 0,
        .pid = 0,
        .itf_num = 1,
        /* 如果设置为 0，则使用默认值 */
        .rx_buffer_size = 0,
        .tx_buffer_size = 0,
        .cbs = {
            .connect = cdc_connect_cb,
            .disconnect = cdc_disconnect_cb,
            .user_data = NULL
        },
    };

    usbh_cdc_handle_t handle = NULL;
    usbh_cdc_create(&dev_config, &handle);
    /* 如果用户想要使用多个接口，可以像这样配置 */
    #if (EXAMPLE_BULK_ITF_NUM > 1)
    config.itf_num = 3;
    usbh_cdc_handle_t handle2 = NULL;
    usbh_cdc_create(&dev_config, &handle2);
    #endif

3. 驱动程序初始化后，内部状态机将自动处理 USB 的热插拔。
4. 成功连接后，主机将自动从 CDC 设备接收 USB 数据到内部 ``ringbuffer``，用户可以轮询 ``usbh_cdc_get_rx_buffer_size`` 以读取缓冲数据大小，或者注册接收回调以在数据准备就绪时得到通知。然后 ``usbh_cdc_read_bytes`` 可以用于读取缓冲数据。
5. ``usbh_cdc_write_bytes`` 可以用于向 USB 设备发送数据。数据首先被写入内部传输 ``ringbuffer``，然后在 USB 总线空闲时发送出去。
6. ``usbh_cdc_delete`` 可以删除 USB CDC 设备，释放掉 ringbuffer 以及其他资源。
7. ``usbh_cdc_uninstall`` 可以完全卸载 USB 驱动程序以释放所有资源。

示例代码
-------------------------------

:example:`usb/host/usb_cdc_basic`

API 参考
-------------------------------

.. include-build-file:: inc/iot_usbh_cdc.inc
