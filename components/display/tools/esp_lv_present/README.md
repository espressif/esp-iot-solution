# ESP LV Present

`esp_lv_present` provides the stable LVGL 9 integration for the presentation engine in `esp_display_present`. Public headers are in `include`; internal binding headers in `src` are not application API.

Use local `esp_lv_present`, `esp_display_present`, and LVGL 9.0 or newer within major 9. LVGL 9.0–9.4 uses an independent draw buffer and a copy into the presenter; native pooled and FULL zero-copy creation return `ESP_ERR_NOT_SUPPORTED` on these releases. See [development and validation](../esp_display_present/MIGRATION_STATUS.md). Source migration is not a Registry release.

When using Component Manager with these local sources, explicitly select **both** local component paths in the application manifest. The current presenter version constraint also matches an older Registry package that does not contain the private bridge required by this binding. Do not publish these manifests unchanged; the repository contains the tested setup and remaining version work in `esp_display_present/dev/PACKAGING_RESULTS.md`.

On LVGL 9.5 and newer, FULL and native 0° DIRECT borrow presenter storage before LVGL refresh admission and revoke the pixel addresses before submission or cancellation. They do not allocate a full-screen placeholder canvas. Rotated DIRECT retains its drawing canvas; pooled PARTIAL retains a tile-sized fallback for admission errors. Destroy the binding before stopping or deleting its presenter, and serialize LVGL refresh and binding control calls on the UI thread.
