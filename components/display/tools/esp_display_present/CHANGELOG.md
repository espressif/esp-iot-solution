# Changelog

## 1.1.1 - 2026-09-30

- Add a hardware-free panel framebuffer requirement query that shares mode,
  rotation and TE policy validation with presenter creation.
- Remove the LVGL adapter dependency from presenter examples and size the
  benchmark's panel buffers for its supported mode and rotation matrix.
- Recheck TE phase eligibility when a current-scan transfer window opens,
  avoiding an unnecessary wait for the next GPIO edge after fast rendering.
  Keep the existing phase-window bounds and revalidate every wakeup.

## 1.1.0 - 2026-09-28

- Overlap partition and TE tile copies with rendering when a second draw buffer
  is available; worker allocation falls back to synchronous copies.
- Fix RGB panels locking fast producers to every other refresh.
- Speed up `DOUBLE_DIRECT` damage sync and the RGB565 byte swap.

## 1.0.2 - 2026-09-04

- Host the component in esp-iot-solution under `components/display/tools/esp_display_present`.
- Relicense the component to Apache-2.0 to match other esp-iot-solution display tools.
- Add the LVGL present benchmark example and a Unity test app.
- Move the GSP/LVGL presenter handoff into ``examples/display/gui/gsp_lvgl_present_handoff``.
- Add ``esp_lv_present`` as the LVGL binding for this presenter.

## 1.0.1 - 2026-09-03

- Rework TE-synchronized presentation around an inline double-buffer pipeline,
  allowing rendering to overlap an in-flight panel transfer without a worker
  task.
- Share buffer repair, pooled-buffer ownership, and transfer-fence handling
  across framebuffer and TE presentation paths.
- Make presenter quiesce wait safely for the final transfer-complete ISR, which
  prevents Display Off from deadlocking while a frame is in flight.
- Remove the obsolete `te_inline_submit` configuration flag; TE composition now
  uses the inline submission path directly.
- Add an on-target runtime benchmark matrix for every panel-compatible
  presentation mode and rotation, including the LVGL DIRECT contract.
- Harden the GSP/LVGL handoff stress app with bounded transition retries,
  zero-frame detection, and fail-closed presenter ownership.

## 1.0.0 - 2026-08-27

- Initial stable release of the renderer-independent ESP LCD presentation and
  tearing-control component.
