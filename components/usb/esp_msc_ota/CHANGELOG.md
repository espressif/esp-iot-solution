# changelog

## v2.0.0 - 2026-6-25

### Breaking Changes:

* `esp_msc_host` no longer posts events through the default `esp_event` loop. `ESP_MSC_HOST_EVENT` event base is removed; register `event_cb`/`event_cb_arg` in `esp_msc_host_config_t` instead.
* Renamed host event enums: `ESP_MSC_HOST_INSTALL` → `ESP_MSC_HOST_DEVICE_INSTALL`, `ESP_MSC_HOST_UNINSTALL` → `ESP_MSC_HOST_DEVICE_UNINSTALL`.
* Removed the global `msc_read_semaphore`. Use `esp_msc_host_lock()` / `esp_msc_host_unlock()` to serialize file access instead.
* `esp_msc_host_uninstall()` now returns `ESP_ERR_INVALID_STATE` if an MSC device is still connected or mounted; unplug the device first.
* `esp_msc_ota_config_t` now requires the new `host_handle` field (returned by `esp_msc_host_install()`). The `skip_msc_connect_wait` field is removed and OTA always waits on the host's VFS mount state via `esp_msc_host_wait_mounted()`.
* Removed `esp_msc_ota_set_msc_connect_state()`; the OTA layer tracks mount state through the host handle.
* `esp_msc_ota_begin()` and `esp_msc_ota()` now take `const esp_msc_ota_config_t *`.
* Removed the internal header `esp_msc_ota_help.h` (and its `MSC_OTA_CHECK*` macros). Use `esp_check.h` / `ESP_RETURN_ON_*` in your own code if you depended on them.

### Features:

* Support ESP32-S31 target.
* New `esp_msc_host` APIs: `esp_msc_host_is_mounted()`, `esp_msc_host_wait_mounted()`, `esp_msc_host_lock()`, `esp_msc_host_unlock()`.
* Add `skip_init_usb_host_driver` to `esp_msc_host_config_t` for applications that manage the USB Host Library and its event-handling task themselves.
* `esp_msc_ota_perform()` now detects device disconnection during the transfer and dispatches `ESP_MSC_OTA_FAILED` on every error path, giving applications a single reliable failure notification.

## v1.1.0 - 2024-12-24

* Add the `skip_msc_connect_wait` configuration to allow users to mount the file system manually.

## v1.0.0 - 2024-8-15

* Publish the official version

## v0.1.2 - 2024-3-8

### Bug Fix

* Fix usb_host_uninstall fail problem.

## v0.1.1 - 2024-2-1

* Add example: MSC USB OTA

## v0.1.0 - 2023-8-15

### Enhancements:

* Support MSC hot-plug
* Support MSC OTA process management
