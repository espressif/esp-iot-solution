# Component: Iot ETH

## Overview

iot_eth is primarily designed for Ethernet controllers with USB or SPI/UART interfaces. It does not involve the configuration details of the MAC PHY, but only focuses on the controller's protocol.

## Example

https://github.com/espressif/esp-iot-solution/tree/master/examples/usb/host/usb_rndis_4g_module

## How to enable IPv6

Enable `CONFIG_LWIP_IPV6` in menuconfig. If using PPP protocol, enable `CONFIG_PPP_IPV6` and `CONFIG_LWIP_IPV6_AUTOCONFIG`.
