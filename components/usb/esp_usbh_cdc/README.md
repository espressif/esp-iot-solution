* [中文版本](./README_cn.md)

## USB Host CDC

This component implements a simple version of the USB CDC host function. It only retains the default control endpoint and two bulk transfer endpoints, which streamlines the enumeration logic of the USB host. Users only need to bind the USB CDC device endpoint address to achieve fast Initialization. The component is suitable for the CDC-like vendor classes that require high startup speed.

The design logic of the component API is similar to the [ESP-IDF UART driver](https://docs.espressif.com/projects/esp-idf/en/latest/esp32s2/api-reference/peripherals/uart.html) interface, which can directly replace the UART interface to realize the update of the original code from UART to USB.

## API Guide

1. Using `usbh_cdc_driver_install` to configure and start internal USB tasks, most importantly the CDC bulk endpoint addresses `bulk_in_ep_addr` and `bulk_out_ep_addr` is required to be specified for communication, and the tramsfer buffer memory size needs to be configured too. user is allowed to config descriptor detials too through `bulk_in_ep` and `bulk_out_ep`.

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

2. After the driver initialization, the internal state machine will automatically handle the hot plug of the USB, and user can also configure the hot plug related callback function `conn_callback` `disconn_callback`.
3. `usbh_cdc_wait_connect` can be used to block task until USB CDC Device is connected or timeout.
4. After successfully connected, the host will automatically receive USB data from CDC device to the internal `ringbuffer`, user can poll `usbh_cdc_get_buffered_data_len` to read buffered data size or register a receive callback to get notified when data is ready. Then `usbh_cdc_read_bytes` can be used to read buffered data out.
5. `usbh_cdc_write_bytes` can be used to send data to USB Device. The data is first written to the internal transmit `ringbuffer`，then will be sent out during USB bus free.
6. `usbh_cdc_driver_delete` can uninstall the USB driver completely to release all resources.

## Examples

* [usb host cdc basic example](../../../examples/usb/host/usb_cdc_basic)
* [usb host cdc 4G modem](../../../examples/usb/host/usb_cdc_4g_module)