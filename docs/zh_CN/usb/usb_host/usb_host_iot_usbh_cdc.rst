USB 主机 CDC 
=====================

:link_to_translation:`en:[English]`

``iot_usbh_cdc`` 组件实现了一个 USB 主机 CDC 驱动的简化版本。该 API 的设计类似于 `ESP-IDF UART 驱动程序 <https://docs.espressif.com/projects/esp-idf/en/latest/esp32s3/api-reference/peripherals/uart.html>`_，可在一些应用中替代 UART，快速实现从 UART 到 USB 的迁移。

使用指南
---------------

1. 使用 ``usbh_cdc_driver_install`` 配置，用户可以简单配置 bulk 端点地址和内部 ringbuffer 的大小，除此之外，用户还可以配置热插拔相关的回调函数 ``conn_callback`` ``disconn_callback``：

.. code:: c

    /* 安装 USB 主机 CDC 驱动程序，配置 bulk 端点地址和内部 ringbuffer 的大小 */
    static usbh_cdc_config_t config = {
        /* use default endpoint descriptor with user address */
        .bulk_in_ep_addr = EXAMPLE_BULK_IN_EP_ADDR,
        .bulk_out_ep_addr = EXAMPLE_BULK_OUT_EP_ADDR,
        .rx_buffer_size = IN_RINGBUF_SIZE,
        .tx_buffer_size = OUT_RINGBUF_SIZE,
        .conn_callback = usb_connect_callback,
        .disconn_callback = usb_disconnect_callback,
    };
    /* 如果用户想要使用多个接口，可以像这样配置 */
    #if (EXAMPLE_BULK_ITF_NUM > 1)
    config.itf_num = 2;
    config.bulk_in_ep_addrs[1] = EXAMPLE_BULK_IN1_EP_ADDR;
    config.bulk_out_ep_addrs[1] = EXAMPLE_BULK_OUT1_EP_ADDR;
    config.rx_buffer_sizes[1] = IN_RINGBUF_SIZE;
    config.tx_buffer_sizes[1] = OUT_RINGBUF_SIZE;
    #endif

    /* 安装 USB 主机 CDC 驱动程序 */
    usbh_cdc_driver_install(&config);

    /* 等待 USB 设备连接 */
    usbh_cdc_wait_connect(portMAX_DELAY);

2. 驱动程序初始化后，内部状态机将自动处理 USB 的热插拔。
3. ``usbh_cdc_wait_connect`` 可以用于阻塞任务，直到 USB CDC 设备连接或超时。
4. 成功连接后，主机将自动从 CDC 设备接收 USB 数据到内部 ``ringbuffer``，用户可以轮询 ``usbh_cdc_get_buffered_data_len`` 以读取缓冲数据大小，或者注册接收回调以在数据准备就绪时得到通知。然后 ``usbh_cdc_read_bytes`` 可以用于读取缓冲数据。
5. ``usbh_cdc_write_bytes`` 可以用于向 USB 设备发送数据。数据首先被写入内部传输 ``ringbuffer``，然后在 USB 总线空闲时发送出去。
6. ``usbh_cdc_driver_delete`` 可以完全卸载 USB 驱动程序以释放所有资源。
7. 如果配置多个 CDC 接口，每个接口都包含一个 IN 和 OUT 端点。用户可以使用 ``usbh_cdc_itf_read_bytes`` 和 ``usbh_cdc_itf_write_bytes`` 与指定的接口通信。

示例代码
-------------------------------

:example:`usb/host/usb_cdc_basic`

API 参考
-------------------------------

.. include-build-file:: inc/iot_usbh_cdc.inc
