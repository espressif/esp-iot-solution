# ESP Display Present

`esp_display_present` provides the stable `esp_display_presenter_*` API, platform resource assembly, and one presentation engine. `esp_lv_present` provides the LVGL binding to that engine. Existing applications keep their presenter API and own the panel, IO, and bus resources.

Public headers are in `include`; internal engine headers in `src` are not application API. The previously public geometry header and its point/size types are no longer exposed; consumers of those interfaces require migration.

For local component discovery add `esp_display_present` and, for LVGL, `esp_lv_present`; neither requires a V2 component. Registry publication and external compatibility have not been established by this source migration. See [current development and validation](MIGRATION_STATUS.md) and the [historical transition design (archive)](dev/ARCHIVE.md).

ESP32-S3 no longer forces whole framebuffer engine and pixel-copy objects into IRAM. The `CONFIG_ESP_DISPLAY_PRESENT_FB_IRAM` option and its linker fragment have been removed to recover internal memory; explicit ISR/pixel-operation `IRAM_ATTR` annotations remain. The earlier IRAM placement measurements are retained as [historical test conditions and limits (archive)](dev/ARCHIVE.md).

`CONFIG_ESP_DISPLAY_PRESENT_GRAM_BACKGROUND` optionally moves synchronous internal-tile GRAM submissions to the existing engine backend worker, releasing the engine gate during driver calls without forcing a pixel copy. It is disabled by default; rotated and PSRAM tile paths keep their existing scheduling. `CONFIG_ESP_DISPLAY_PRESENT_GRAM_BACKGROUND_PRIORITY` sets the experimental backend worker priority, defaulting to 7. See [conditions, RAM cost, and measured results (archive)](dev/ARCHIVE.md).

See [mode contracts, backend strategies and LCD adapters](docs/MODE_ARCHITECTURE.md) for scanout-only RGB/DSI modes, AUTO selection and completion semantics. RGB/DSI NONE and single-framebuffer scanout are unsupported; Panel-IO NONE and TE behavior are unchanged.

See [TE timing and scheduling](docs/TE_TIMING.md) for automatic measurement, the shared window model, and compatibility boundaries.

See [current board benchmarks and host validation](dev/VALIDATION.md) for the tested commit, configurations, scene results, and limits.

The [historical mode and rotation matrix (archive)](dev/ARCHIVE.md) records the original P4 cross-axis defect. The later geometry correction and hardware/PPA regressions are linked from [migration status](MIGRATION_STATUS.md); do not treat that historical defect as an unresolved current result. S31 GRAM instances without rotated destination storage may still reject runtime rotation.

RGB framebuffer completion is selected inside the scanout adapter. Applications
configure the LCD panel normally; there is no public `rgb.scanout` setting.
The driver capability query is authoritative when available. On clean IDF
versions without that query, a private BSP binding can supply actual panel facts
from initialization: dimensions, framebuffer count/format, bounce capacity,
continuous refresh and driver transforms. The binding reports hardware facts,
not a requested protocol. Known invalid facts fail closed.

A verified S31 non-bounce AXI path retains requested completion received before
the driver returns. Bounce completion retains its periodic ISR-join fence.
S3 non-bounce EOF is not accepted as a multi-buffer retirement guarantee.
Without driver/BSP facts, only the existing confirmed-bounce fallback remains;
a generic borrowed handle alone does not enable non-bounce support. No global
LCD interception, private IDF struct layout or IDF patch is used. See the
[RGB handoff regression](dev/test_apps/rgb_handoff/README.md).

Test projects, board profiles, LVGL benchmark and adapter comparison: [dev/README.md](dev/README.md).
