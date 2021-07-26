## esp_usbh_cdc 组件说明

该组件实现了简易版本的 USB CDC 主机功能，仅保留了默认控制传输端点和 2 个批量传输端点，精简了 USB 主机的枚举逻辑，用户只需绑定 USB CDC 设备端点地址即可实现快速的初始化，适用于对启动速度要求较高的自定义类设备。

该组件 API 的设计逻辑与 ESP-IDF UART 驱动接口类似，可直接替换 UART 接口，实现原有代码 UART-> USB 的更新。

## API 使用说明

1. 使用 `usbh_cdc_driver_install` 配置并启动 USB 任务，初始化参数如下所示，在示例基础上一般只需修改端点地址 `bEndpointAddress` 即可;

    ```
    static usb_desc_ep_t bulk_out_ep_desc = {
        .bLength = sizeof(usb_desc_ep_t),
        .bDescriptorType = USB_B_DESCRIPTOR_TYPE_ENDPOINT,
        .bEndpointAddress = 0x01,       //EP 1 OUT
        .bmAttributes = USB_BM_ATTRIBUTES_XFER_BULK,
        .wMaxPacketSize = 64,           //MPS of 64 bytes
        .bInterval = 0,
    };

    static usb_desc_ep_t bulk_in_ep_desc = {
        .bLength = sizeof(usb_desc_ep_t),
        .bDescriptorType = USB_B_DESCRIPTOR_TYPE_ENDPOINT,
        .bEndpointAddress = 0x81,       //EP 2 IN
        .bmAttributes = USB_BM_ATTRIBUTES_XFER_BULK,
        .wMaxPacketSize = 64,           //MPS of 64 bytes
        .bInterval = 0,
    };

    static usbh_cdc_config_t config = {
        .bulk_in_ep = &bulk_in_ep_desc,
        .bulk_out_ep = &bulk_out_ep_desc,
        .rx_buffer_size = IN_RINGBUF_SIZE,
        .tx_buffer_size = OUT_RINGBUF_SIZE,
    };

    esp_err_t ret = usbh_cdc_driver_install(&config);
    assert(ret == ESP_OK);
    ```

2. 初始化成功之后可使用 `usbh_cdc_write_bytes` 写出 USB 数据，数据默认先写到内部 `ringbuffer`，等到 USB 总线空闲时，自动写出到 USB 总线；
3. 初始化成功之后，主机将自动接收 USB 总线数据到内部 `ringbuffer`，用户程序可在初始化时注册回调函数，也可轮训 `usbh_cdc_get_buffered_data_len` 在内部 `ringbuffer` 收到数据时通过 `usbh_cdc_read_bytes` 将数据读出； 
4. 最后用户可通过 `usbh_cdc_driver_delete` 卸载 USB 程序，将内部资源完全释放。
