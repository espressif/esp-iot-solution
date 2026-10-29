# ChangeLog

## v1.1.0 - 2026-06-30

### Features

* Add input callback support for forwarding extra RX frame information.
* Add ESP-NETIF L2 TAP filtering support for Ethernet netifs.

### Bug Fixes

* Skip L2 TAP filtering for PPP netifs to avoid parsing PPP frames as Ethernet frames.

## v1.0.0 - 2025-10-15

### Bug Fixes

* Add parameter check and improve stability

### Features

* Add support for PPP protocol network devices
* Add support multiple instances of network drivers
* Add support for IPv6

### Breaking Changes

* Remove `user_data` from the `iot_eth_config_t` structure

## v0.1.0 - 2025-04-22

* publish official version
