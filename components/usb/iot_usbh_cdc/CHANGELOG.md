# ChangeLog

## v2.0.1 - 2025-06-17

### Bug Fixes:

* Fix the issue of incorrectly initiating `tx_xfer` multiple times.

## v2.0.0 - 2025-05-20

### Features:

* Support no internal ring buffer mode..

### Break Changes:

* Set rx_buffer_size to 0 to disable internal ring buffer.

* Remove macro IN_RINGBUFFER_SIZE and OUT_RINGBUFFER_SIZE.

## v1.1.1 - 2025-05-13

### Features:

* Skip open usb hub device.

## v1.1.0 - 2025-04-21

### Features:

* Support control transfer and notification transfer.

* Support usbh_cdc_send_custom_request() to send custom request.

## v1.0.4 - 2025-4-1

### Bug Fixes:

* Fix USB host interface release when URB is not cleared.

## v1.0.3 - 2025-3-27

### Bug Fixes:

* Fixed an incorrect `ring_buffer pop` request value of 0.
* Fixed uninitialized structure issue.

## v1.0.2 - 2025-3-13

### Bug Fixes:

* Close 'extern C' block
* flush the ringbuffer when usb disconnect

## v1.0.1 - 2025-2-26

### Bug Fixes:

* Fix the cdc can't open the setting's interface cause crash issue.

## v1.0.0 - 2024-10-31

### Break Changes:

* Remove the dependency on the `iot_usbh` component and use `usb_host_driver` instead.

### API Break Changes:

* Add `usbh_cdc_create()`
* Add `usbh_cdc_delete()`
* Add `usbh_cdc_desc_print()` to print CDC descriptor
* Rename `usbh_cdc_get_itf_state()` to `usbh_cdc_get_state()`
* Remove `usbh_cdc_wait_connect()`
* Remove `usbh_cdc_itf_write_bytes()`
* Remove `usbh_cdc_get_buffered_data_len()`
* Remove `usbh_cdc_itf_get_buffered_data_len()`
* Remove `usbh_cdc_itf_read_bytes()`

## v0.2.2 - 2023-12-08

* Add doc and example

## v0.2.1 - 2023-11-23

* Fix possible cmake_utilities dependency issue

## v0.2.0 - 2023-10-09

* add hardware test
* decrease the stack priority to `usb base` level

## v0.1.3 - 2023-04-17

### Bug Fixes:

* Update iot_usbh version to 0.1.2, fix build error with latest ESP-IDF `release/v4.4`

## v0.1.2 - 2023-02-21

### Bug Fixes:

* Fix the multi-thread access error caused by `usbh_cdc_driver_delete()`

## v0.1.1 - 2023-02-13

* Support IDF5.0
* Change dependency iot_usbh to public

## v0.1.0 - 2023-02-02

* initial version
