## Unreleased — unified presentation engine

- Fix fault classification at the presenter facade: only an original
  `ESP_ERR_TIMEOUT` is reported as `ESP_DISPLAY_PRESENT_FAULT_TRANSFER_TIMEOUT`.
  Other accepted transfer/copy failures now report the new
  `ESP_DISPLAY_PRESENT_FAULT_TRANSFER_FAILED`, and the new
  `esp_display_presenter_get_fault_error()` returns the original error before
  classification. This changes the reported reason for non-timeout failures;
  applications must not treat the new reason as a timeout. The classification
  contract and matrix are documented in `STATE_MACHINE.md` and covered by
  `fault_classification_matrix` in `dev/tests/test_legacy_facade.c`.
- Keep the main presenter API and component name, backed by one internal engine.
- Remove the superseded implementation and private-source dependencies.
- Require multi-framebuffer scanout for RGB/MIPI; reject NONE and single-FB
  configurations. AUTO selects double/triple PARTIAL against the actual pool.
- Remove full-screen placeholder canvases for LVGL FULL/native DIRECT.
- Move the former public geometry interfaces to internal headers; consumers of
  those interfaces require migration.
- See `COMPATIBILITY.md`, `docs/MODE_ARCHITECTURE.md` and `MIGRATION_STATUS.md`
  for current contracts. Historical migration records are under `dev/history/`.

# Changelog

## 1.2.0 - 2026-10-27

- Add correlated frame receipts, coherent transfer/display status and tracking cancellation.
- Track RGB/MIPI VSYNC on demand and preserve frame IDs across rotation.
- Wake TE at phase boundaries on IDF 6.2+, with callback-safe teardown and edge/tick fallback.

## 1.1.2 - 2026-10-09

- Align renderer buffers and expose their guaranteed base alignment.
- Fix framebuffer retirement races and early RGB completion.

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
