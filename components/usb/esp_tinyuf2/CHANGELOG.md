# ChangeLog

## v0.2.1 - 2024-04-22

* Fix build with latest ESP-IDF (USB Hal: Change name of usb_phy HAL files to usb_fsls_phy)

## v0.2.0 - 2023-07-17

* Support `esp_tinyuf2_uninstall` API (Restore the USB hardware to default state, Not teardown TinyUSB stack)
* Support target test

## v0.1.0 - 2023-07-05

* Support USB Console for log (disabled by default)

## v0.0.2 - 2023-07-03

* Add Linux INI file write workaround

## v0.0.1 - 2023-06-26

* initial version
* Support update APP through UF2
* Support modify NVS through UF2
