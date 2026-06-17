# Simplified BSPs for ESP-DL Examples

This directory holds **slim board support packages** shared by the examples under `examples/ai/esp_dl/`. They are intentionally minimal: only the parts a typical camera + LCD AI demo needs (camera, LCD, touch, shared I2C) are kept — audio, SD card, NAND, USB host, HDMI, etc. are stripped out.

Both boards expose the **same uniform API**, so example code stays board-agnostic and the target is selected purely by `idf_component.yml` rules.

## Boards

| Component | Target | Camera | LCD | Touch |
|-----------|--------|--------|-----|-------|
| `esp32_p4_function_ev_board` | `esp32p4` | MIPI-CSI (`esp_video`) | MIPI-DSI (`ek79007` / `ili9881c`) + LVGL via `esp_lvgl_port` | GT911 |
| `esp32_s31_korvo` | `esp32s31` | DVP (`esp_video`) | RGB 800x480 + LVGL via `esp_lvgl_adapter` | GT1151 |

## Uniform API

Both boards provide (`#include "bsp/esp-bsp.h"`):

```c
/* Display */
lv_display_t *bsp_display_start(void);            /* LCD + LVGL all-in-one */
esp_err_t     bsp_display_new(...);               /* esp_lcd panel only (no LVGL) */
bool          bsp_display_lock(int32_t timeout_ms);   /* 0 == wait forever */
void          bsp_display_unlock(void);
esp_err_t     bsp_display_brightness_set(int percent);
i2c_master_bus_handle_t bsp_i2c_get_handle(void);

/* Camera (V4L2). bsp_camera_start() just brings up esp_video; the example keeps the V4L2 capture loop and opens BSP_CAMERA_DEVICE. */
esp_err_t bsp_camera_start(const bsp_camera_cfg_t *cfg);
#define   BSP_CAMERA_DEVICE   /* "/dev/video0" (CSI) or "/dev/video2" (DVP) */
```

## Use in a new example

Add the boards to `main/idf_component.yml`, selected by target:

```yaml
dependencies:
  esp32_p4_function_ev_board:
    override_path: ../../common_components/esp32_p4_function_ev_board
    rules:
      - if: "target == esp32p4"
  esp32_s31_korvo:
    override_path: ../../common_components/esp32_s31_korvo
    rules:
      - if: "target == esp32s31"
```

`esp_video` is pulled in as a public dependency of whichever board is selected, so the example does not need to depend on it directly.
