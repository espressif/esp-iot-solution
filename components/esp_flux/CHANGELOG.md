# ChangeLog

## v0.2.0 - 2026-10-29

### Enhancements:

* Add connection-handle-based feed and MTU helpers that pin GATT sessions for the duration of each operation

## v0.1.0 - 2026-10-28

### Feature:

* `flux_callbacks_t.progress_cb` and `ble_session_callbacks_t.progress_cb` take `stream_id` before the byte counts: `(session, stream_id, transferred, total)`. A callback still declared as `(session, transferred, total)` is a different function type. If it is compiled anyway, `stream_id` is read as `transferred` and `transferred` is read as `total`. Update the callback; there is no source-compatible wrapper, because concurrent streams need the id. `gmp_flux_progress()` and `gmp_flux_gatt_progress()` in `esp_gmp` already forward `stream_id`.

## v0.0.3~2 - 2026-10-28

### Examples:

* `examples/esp_flux/back_client.c`: GATT Read uses a caller-owned 512-byte buffer instead of `data = NULL`.

## v0.0.3~1 - 2026-10-15

### Documentation:

* Correct transport and GATT header paths in README (no behavior changes).

## v0.0.3 - 2026-08-03

### Bug Fixes:

* Call `ble_npl_event_deinit` before freeing the GATT destroy event in `gatt_session_destroy_event_fn`, avoiding an NPL event resource leak after scheduled session teardown.

## v0.0.2 - 2026-07-15

### Bug Fixes:

* Notify adapters of in-flight zero-copy sends dropped during `flux_session_destroy` via `session_complete_cb` (`ESP_ERR_INVALID_STATE`), so caller-owned source buffers can be released.
* Add `gatt_session_schedule_destroy()` to post GATT session teardown onto the NimBLE host event queue, avoiding reentrancy when destroy is requested outside the host task.

## v0.0.1 - 2026-06-12

### Enhancements:

* Initial release of the ESP Flux transport and GATT session component.
