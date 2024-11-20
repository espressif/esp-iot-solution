# USB Host CDC-ACM Class Driver

[![Component Registry](https://components.espressif.com/components/espressif/usb_host_cdc_acm/badge.svg)](https://components.espressif.com/components/espressif/usb_host_cdc_acm)
![maintenance-status](https://img.shields.io/badge/maintenance-passively--maintained-yellowgreen.svg)

This component contains an implementation of a USB CDC-ACM Host Class Driver that is implemented on top of the [USB Host Library](https://docs.espressif.com/projects/esp-idf/en/latest/esp32s2/api-reference/peripherals/usb_host.html).

## Supported Devices

The CDC-ACM Host driver supports the following types of CDC devices:

1. CDC-ACM devices
2. CDC-like vendor specific devices (usually found on USB to UART bridge devices or cellular modems)

### CDC-ACM Devices

The CDC-ACM Class driver supports CDC-ACM devices that meet the following requirements:
- The device class code must be set to the CDC class `0x02` or implement Interface Association Descriptor (IAD)
- The CDC-ACM must contain the following interfaces:
    - A Communication Class Interface containing a management element (EP0) and may also contain a notification element (an interrupt endpoint). The driver will check this interface for CDC Functional Descriptors.
    - A Data Class Interface with two BULK endpoints (IN and OUT). Other transfer types are not supported by the driver

### CDC-Like Vendor Specific Devices

The CDC-ACM Class driver supports CDC-like devices that meet the following requirements:
- The device class code must be set to the vendor specific class code `0xFF`
- The device needs to provide and interface containing the following endpoints:
    - (Mandatory) Two Bulk endpoints (IN and OUT) for data
    - (Optional) An interrupt endpoint (IN) for the notification element

For CDC-like devices, users are responsible for ensuring that they only call APIs (e.g., `cdc_acm_host_send_break()`) that are supported by the target device.


## Usage

The following steps outline the typical API call pattern of the CDC-ACM Class Driver

1. Install the USB Host Library via `usb_host_install()`
2. Install the CDC-ACM driver via `cdc_acm_host_install()`
3. Call `cdc_acm_host_open()` to open a CDC-ACM/CDC-like device. This function will block until the target device is connected or timeout
4. To transmit data, call `cdc_acm_host_data_tx_blocking()`
5. When data is received, the driver will automatically run the receive data callback
6. An opened device can be closed via `cdc_acm_host_close()`
7. The CDC-ACM driver can be uninstalled via `cdc_acm_host_uninstall()`

Use `CDC_HOST_ANY_*` macros to signal to `cdc_acm_host_open()` function that you don't care about the device's VID and PID. In this case, first USB device will be opened. It is recommended to use this feature if only one device can ever be in the system (there is no USB HUB connected).

## Examples

- For an example with a CDC-ACM device, refer to [cdc_acm_host](https://github.com/espressif/esp-idf/tree/master/examples/peripherals/usb/host/cdc/cdc_acm_host)
- For an example with Virtual COM devices, refer to [cdc_acm_vcp](https://github.com/espressif/esp-idf/tree/master/examples/peripherals/usb/host/cdc/cdc_acm_vcp)
- For examples with [esp_modem](https://components.espressif.com/components/espressif/esp_modem), refer to [esp_modem examples](https://github.com/espressif/esp-protocols/tree/master/components/esp_modem/examples)
