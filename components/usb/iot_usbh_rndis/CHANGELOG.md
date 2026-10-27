# ChangeLog

## v0.5.2 - 2026-10-21

### Bug Fixes:

* Fix link recovery after USB reconnection by recognizing standard RNDIS response notifications and retaining CDC notification compatibility.
* Track pending response notifications to avoid losing coalesced events and repeatedly reading unchanged device responses.
* Stop asynchronous reads on empty responses or transfer errors, and clear pending notifications on disconnect.

### Dependencies:

* Require `iot_usbh_cdc` version `^3.1.1` for notification validation and control response length fixes.

## v0.5.1 - 2026-10-09

### Bug Fixes:

* Fixed `out_transfer_buffer_size` to Tx a full sized ethernet packet

## v0.5.0 - 2026-06-30

### Features:

* Add support for the `iot_eth` input callback with extra RX frame information.

## v0.4.0 - 2026-06-11

### Features:

* Support ESP32-S31.

### Bug Fixes:

* Fixed RNDIS control polling for devices that do not reliably send response notifications.
* Fixed QUERY transfer length and response information-buffer offset parsing.
* Handled interleaved indicate and keepalive messages during command completions.

## v0.3.1 - 2025-12-31

### Bug Fixes:

* Fixed the issue of excessively long waiting time when obtaining the IP address.

## v0.3.0 - 2025-10-15

### Bug Fixes:

* Fixed the problem of sticky packets when running with large traffic
* Suppress some warning logs

### Break Changes:

* Modify the `iot_usbh_rndis_config_t` to force auto detect the USB device

## v0.2.0 - 2025-05-20

* Optimize uplink speed from 0.9Mbps to 5Mbps

## v0.1.0 - 2025-04-22

* publish official version
