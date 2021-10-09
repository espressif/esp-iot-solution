* [English version](./README.md)

## USB Host CDC

该组件实现了简易版本的 USB CDC 主机功能，仅保留了默认控制传输端点和 2 个批量传输端点，精简了 USB 主机的枚举逻辑，用户只需绑定 USB CDC 设备端点地址即可实现快速的初始化，适用于对启动速度要求较高的自定义类设备。

该组件 API 的设计逻辑与 [ESP-IDF UART 驱动](https://docs.espressif.com/projects/esp-idf/en/latest/esp32s2/api-reference/peripherals/uart.html)接口类似，可直接替换 UART 接口，实现原有代码 UART-> USB 的更新。

## API 使用说明

1. 使用 `usbh_cdc_driver_install` 来配置和启动内部 USB 任务，最重要的是需要指定 CDC 批量端点地址 `bulk_in_ep_addr` 和 `bulk_out_ep_addr` 用于通信，并且还需要配置 tramsfer 缓冲区内存大小。 用户也可以通过 `bulk_in_ep` 和 `bulk_out_ep` 配置整个描述符详细信息。

    ```
    /* @brief install usbh cdc driver with bulk endpoint configs and size of internal ringbuffer*/
    static usbh_cdc_config_t config = {
        /* use default endpoint descriptor with user address */
        .bulk_in_ep_addr = EXAMPLE_BULK_IN_EP_ADDR,
        .bulk_out_ep_addr = EXAMPLE_BULK_OUT_EP_ADDR,
        .rx_buffer_size = IN_RINGBUF_SIZE,
        .tx_buffer_size = OUT_RINGBUF_SIZE,
        .conn_callback = usb_connect_callback,
        .disconn_callback = usb_disconnect_callback,
    };

    /* install USB host CDC driver */
    usbh_cdc_driver_install(&config);

    /* Waitting for USB device connected */
    usbh_cdc_wait_connect(portMAX_DELAY);
    ```

2. 初始化成功以后，内部状态机会自动处理 USB 的热插拔，用户也可以在初始化时配置热插拔相关回调函数 `conn_callback` 和 `disconn_callback`。
3. `usbh_cdc_wait_connect` 可用于阻塞任务，直到 USB CDC 设备连接或超时。
4. 连接成功后，主机会自动从 CDC 设备接收 USB 数据到内部 `ringbuffer`，用户可以轮询`usbh_cdc_get_buffered_data_len` 来读取缓存数据的大小或注册接收回调以在数据准备好时得到通知。然后可以使用 `usbh_cdc_read_bytes` 来读取缓冲的数据。
5. `usbh_cdc_write_bytes` 可用于向 USB 设备发送数据。 数据首先写入内部 `ringbuffer`，然后在 USB 总线空闲时发送出去。
5、`usbh_cdc_driver_delete` 可以完全卸载 USB 驱动，释放所有资源。

## Examples

* [CDC 基础功能示例](../../../examples/usb/host/usb_cdc_basic)
* [CDC 4G 上网](../../../examples/usb/host/usb_cdc_4g_module)