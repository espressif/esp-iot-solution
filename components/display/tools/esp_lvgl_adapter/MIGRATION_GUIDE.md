# Migration Guide: esp_lvgl_port -> esp_lvgl_adapter

This document helps you migrate projects from `esp_lvgl_port` to `esp_lvgl_adapter`, highlighting API changes, configuration differences, and additional features provided by the adapter.

## Scope

- ESP-IDF >= 5.5
- LVGL v8 or v9
- `esp_lcd` / `esp_lcd_touch` based drivers

## What the Adapter Adds

Compared to `esp_lvgl_port`, the adapter provides:

- Unified display registration for MIPI DSI / RGB / SPI / I2C / I80 / QSPI.
- Tearing avoidance modes and TE synchronization support (interface-specific).
- Thread-safe LVGL access (`esp_lv_adapter_lock()` / `esp_lv_adapter_unlock()`).
- Sleep prepare/recover to detach and rebind LCD hardware without losing UI state.
- Optional modules: filesystem bridge, image decoder, FreeType fonts.
- FPS statistics and Dummy Draw options.
- Multi-display management in one runtime.

## Migration Steps

### 1) Replace Component Dependencies

Update `idf_component.yml`:

```yaml
dependencies:
  espressif/esp_lvgl_adapter: "*"
  # remove: espressif/esp_lvgl_port
```

Optional dependencies (enable only if needed):

- `espressif/button`
- `espressif/knob`
- `espressif/esp_lv_fs`
- `espressif/esp_lv_decoder`
- LVGL FreeType support (see Kconfig in this component)

### 2) Kconfig Changes

`esp_lvgl_port` had `LVGL_PORT_ENABLE_PPA`. In the adapter, PPA usage is controlled per display via `profile.enable_ppa_accel`.

Adapter options (enable as needed):

- `CONFIG_ESP_LVGL_ADAPTER_ENABLE_BUTTON`
- `CONFIG_ESP_LVGL_ADAPTER_ENABLE_KNOB`
- `CONFIG_ESP_LVGL_ADAPTER_ENABLE_FS`
- `CONFIG_ESP_LVGL_ADAPTER_ENABLE_DECODER`
- `CONFIG_ESP_LVGL_ADAPTER_ENABLE_FREETYPE`
- `CONFIG_ESP_LVGL_ADAPTER_ENABLE_FPS_STATS`

### 3) Initialization

**Old:**

```c
const lvgl_port_cfg_t cfg = ESP_LVGL_PORT_INIT_CONFIG();
ESP_ERROR_CHECK(lvgl_port_init(&cfg));
```

**New:**

```c
esp_lv_adapter_config_t cfg = ESP_LV_ADAPTER_DEFAULT_CONFIG();
ESP_ERROR_CHECK(esp_lv_adapter_init(&cfg));
ESP_ERROR_CHECK(esp_lv_adapter_start());
```

Common field mapping:

| lvgl_port_cfg_t | esp_lv_adapter_config_t | Notes |
| --- | --- | --- |
| task_priority | task_priority | Task priority |
| task_stack | task_stack_size | Stack size (bytes) |
| task_affinity | task_core_id | Core affinity |
| timer_period_ms | tick_period_ms | LVGL tick period |
| task_max_sleep_ms | task_max_delay_ms | Max task delay |
| task_stack_caps | stack_in_psram | No direct mapping; adapter uses a boolean |

`esp_lv_adapter_config_t` also adds `task_min_delay_ms` (keep defaults unless you have a reason to tune).

### 4) Display Registration

#### 4.1 Select Interface and Tearing Mode

RGB/MIPI DSI require `num_fbs` before LCD init. Compute it using the adapter API:

```c
esp_lv_adapter_tear_avoid_mode_t tear_mode = ESP_LV_ADAPTER_TEAR_AVOID_MODE_DEFAULT_RGB;
esp_lv_adapter_rotation_t rotation = ESP_LV_ADAPTER_ROTATE_0;
uint8_t num_fbs = esp_lv_adapter_get_required_frame_buffer_count(tear_mode, rotation);
// Pass num_fbs into esp_lcd panel config
```

#### 4.2 Config Structure Changes

**Old focus (port):**

- `buffer_size` in pixels
- `double_buffer`
- `rotation.swap_xy / mirror_x / mirror_y`
- `flags` (DMA, PSRAM, direct mode, full refresh, swap bytes)

**New focus (adapter):**

- `profile.buffer_height` (stripe height for LVGL draw buffer)
- `profile.use_psram`
- `profile.require_double_buffer`
- `tear_avoid_mode` (buffering strategy and refresh behavior)
- `te_sync` (TE-based sync for SPI/I80/QSPI)

**Recommendation:** use the provided `ESP_LV_ADAPTER_DISPLAY_*_DEFAULT_CONFIG(...)` macros as-is. Avoid modifying the generated struct unless you have a clear, documented reason. This prevents missing required defaults and keeps configurations aligned with supported interfaces.

Conversion helper:

```
buffer_height = buffer_size / hor_res
```

#### 4.3 Example: SPI/I2C/I80/QSPI

**Old:**

```c
const lvgl_port_display_cfg_t disp_cfg = {
    .io_handle = io_handle,
    .panel_handle = panel_handle,
    .buffer_size = HRES * 50,
    .double_buffer = true,
    .hres = HRES,
    .vres = VRES,
    .flags = { .buff_dma = true },
};
lv_display_t *disp = lvgl_port_add_disp(&disp_cfg);
```

**New:**

```c
esp_lv_adapter_display_config_t disp_cfg =
    ESP_LV_ADAPTER_DISPLAY_SPI_WITH_PSRAM_DEFAULT_CONFIG(panel_handle, io_handle, HRES, VRES, ESP_LV_ADAPTER_ROTATE_0);
lv_display_t *disp = esp_lv_adapter_register_display(&disp_cfg);
```

#### 4.4 Example: RGB / MIPI DSI

**Old:**

```c
lv_display_t *disp = lvgl_port_add_disp_rgb(&disp_cfg, &rgb_cfg);
```

**New:**

```c
esp_lv_adapter_display_config_t disp_cfg =
    ESP_LV_ADAPTER_DISPLAY_RGB_DEFAULT_CONFIG(panel_handle, io_handle, HRES, VRES, ESP_LV_ADAPTER_ROTATE_0);
disp_cfg.tear_avoid_mode = ESP_LV_ADAPTER_TEAR_AVOID_MODE_TRIPLE_PARTIAL;
lv_display_t *disp = esp_lv_adapter_register_display(&disp_cfg);
```

### 5) Input Devices

**Touch:**

```c
esp_lv_adapter_touch_config_t touch_cfg = ESP_LV_ADAPTER_TOUCH_DEFAULT_CONFIG(disp, tp);
lv_indev_t *touch = esp_lv_adapter_register_touch(&touch_cfg);
```

**Buttons / Encoder:** enable Kconfig options first.

- `lvgl_port_add_navigation_buttons()` -> `esp_lv_adapter_register_navigation_buttons()`
- `lvgl_port_add_encoder()` -> `esp_lv_adapter_register_encoder()`

### 6) Thread Safety and Refresh

**Old:**

```c
if (lvgl_port_lock(0)) { /* LVGL calls */ lvgl_port_unlock(); }
```

**New:**

```c
if (esp_lv_adapter_lock(-1) == ESP_OK) { /* LVGL calls */ esp_lv_adapter_unlock(); }
```

Immediate refresh is available via `esp_lv_adapter_refresh_now()`.

### 7) Sleep / Resource Detach

The adapter supports preserving UI state while releasing LCD resources:

- `esp_lv_adapter_sleep_prepare()`
- `esp_lv_adapter_sleep_recover()`

This is useful when you need to call `esp_lcd_panel_del()` and later rebind.

## API Mapping

| esp_lvgl_port | esp_lvgl_adapter |
| --- | --- |
| lvgl_port_init | esp_lv_adapter_init + esp_lv_adapter_start |
| lvgl_port_deinit | esp_lv_adapter_deinit |
| lvgl_port_lock / unlock | esp_lv_adapter_lock / unlock |
| lvgl_port_add_disp | esp_lv_adapter_register_display |
| lvgl_port_add_disp_rgb | esp_lv_adapter_register_display + RGB profile |
| lvgl_port_add_disp_dsi | esp_lv_adapter_register_display + MIPI profile |
| lvgl_port_remove_disp | esp_lv_adapter_unregister_display |
| lvgl_port_add_touch | esp_lv_adapter_register_touch |
| lvgl_port_remove_touch | esp_lv_adapter_unregister_touch |
| lvgl_port_add_navigation_buttons | esp_lv_adapter_register_navigation_buttons |
| lvgl_port_add_encoder | esp_lv_adapter_register_encoder |
| lvgl_port_stop / resume | esp_lv_adapter_pause / resume |

`lvgl_port_task_wake()` and `lvgl_port_flush_ready()` have no direct equivalent; the adapter manages LVGL task wakeups and flush lifecycle internally.

## BSP Migration Notes

Most BSPs in `esp-bsp` use `lvgl_port_add_disp()` / `lvgl_port_add_touch()`:

- Replace BSP display init with adapter init + display registration + `esp_lv_adapter_start()`.
- Replace BSP lock/unlock with adapter lock/unlock.
- For RGB/MIPI DSI BSPs, compute `num_fbs` using `esp_lv_adapter_get_required_frame_buffer_count()` and pass it into panel config.

Start with a generic BSP (for example, `esp_bsp_generic`) to validate the migration before applying the pattern to board-specific BSPs.

## Pitfalls and Checks

- RGB/MIPI DSI with `TEAR_AVOID_MODE_NONE` does not allow rotation.
- `buffer_height` too small hurts performance; too large consumes RAM.
- For TE sync on SPI/I80/QSPI, connect the panel TE pin to a GPIO and configure `te_sync`.
- Touch scaling/rotation must match LCD orientation (swap XY/mirror settings).
- `lvgl_port_lock(0)` was an infinite wait; use `esp_lv_adapter_lock(-1)` for the same behavior.

## Post-Migration Checklist

- Run LVGL demos/benchmarks to verify rendering and input.
- If tearing or stutter appears, re-check `tear_avoid_mode` and `num_fbs`.
- Verify that optional modules (FS/decoder/FreeType/FPS stats) are enabled only when needed.
