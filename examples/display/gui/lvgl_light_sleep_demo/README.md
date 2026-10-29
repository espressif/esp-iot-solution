| Supported Targets | ESP32-P4 | ESP32-S3 | ESP32-S31 | ESP32-C3 |
| ----------------- | -------- | -------- | --------- | -------- |

# LVGL Light Sleep Demo

This example demonstrates interface-aware auto sleep with `esp_lvgl_adapter` while keeping LCD control in the application layer.

## Overview

The example uses two different auto sleep patterns:

- **SPI/QSPI**: The adapter runs `ESP_LV_ADAPTER_AUTO_SLEEP_MODE_PAUSE`, pauses the LVGL worker after an idle timeout, releases its PM lock, and lets the example callback put the panel to sleep or blank it.
- **RGB/MIPI DSI**: The adapter runs `ESP_LV_ADAPTER_AUTO_SLEEP_MODE_USER`, and the example callback performs the full sequence `sleep_prepare -> LCD deinit -> esp_light_sleep_start -> LCD init -> sleep_recover`.

The adapter boundary is explicit:

- The adapter decides when LVGL can sleep or wake.
- The example owns LCD operations such as `esp_lcd_panel_disp_sleep()`, `esp_lcd_panel_disp_on_off()`, `hw_lcd_deinit()`, and `hw_lcd_init()`.
- The adapter does not directly call panel sleep APIs.

## How to Use the Example

### Hardware Required

* An ESP32-P4, ESP32-S3, ESP32-S31, or ESP32-C3 development board
* A LCD panel with one of the supported interfaces:
  - **MIPI DSI**
  - **RGB**
  - **QSPI**
  - **SPI**
* Optional touch input or rotary encoder input
* A USB cable for power supply and programming

### Configure the Project

Run `idf.py menuconfig` and navigate to `LVGL Light Sleep Demo Configuration`:

- `Sleep mode`: `Auto sleep` or `Manual sleep`
- `Auto sleep idle timeout (ms)`: Idle time before auto sleep starts
- `Light sleep wake timer (ms)`: Wake timer used by the RGB/MIPI user-managed flow

The demo defaults also enable:

- `CONFIG_PM_ENABLE=y`
- `CONFIG_FREERTOS_USE_TICKLESS_IDLE=y`
- `CONFIG_PM_POWER_DOWN_PERIPHERAL_IN_LIGHT_SLEEP=y`
- `CONFIG_DMA2D_OPERATION_FUNC_IN_IRAM=y`

`CONFIG_PM_POWER_DOWN_PERIPHERAL_IN_LIGHT_SLEEP` is optional (EXPERIMENTAL). It
powers down digital peripherals in light sleep for lower power, at the cost of
extra RAM/heap, and only takes effect on targets with TOP power-domain + PAU
support (ESP32-P4, ESP32-S31, ESP32-C6/C5/C61, ESP32-H2/H4/H21). Because 2D-DMA
has no sleep retention, the adapter guards its power domain across sleep while
this option is on. The demo also works correctly with this option disabled.

### Build and Flash

1. Set the target chip:

```bash
idf.py set-target esp32p4
# or esp32s3 / esp32s31 / esp32c3
```

2. Build, flash and monitor:

```bash
idf.py -p PORT build flash monitor
```

## Runtime Behavior

After flashing, the LCD shows a static status label and one action button.

### Manual Sleep Path

1. Press `Sleep Now`
2. The example runs `esp_lv_adapter_sleep_prepare()`
3. The example deinitializes the LCD and enters `esp_light_sleep_start()`
4. The wake timer resumes execution
5. The example reinitializes the panel and calls `esp_lv_adapter_sleep_recover()`

### SPI/QSPI Path

1. The adapter detects LVGL idle time
2. The example callback sends panel sleep or display-off commands and arms a
   one-shot wake timer
3. The adapter pauses the LVGL worker and releases its PM lock
4. Tickless idle can enter light sleep automatically
5. The wake timer (or any touch/button/encoder activity) calls
   `esp_lv_adapter_request_wake()` to wake the adapter
6. The example callback restores the panel before LVGL resumes

> The wake source is the application's choice. This demo uses a timer so the
> behavior matches the RGB/MIPI flow; a real product can instead arm the input
> GPIO as a light-sleep wake source for activity-driven wake.

### RGB/MIPI DSI Path

1. The adapter detects LVGL idle time
2. The example callback calls `esp_lv_adapter_sleep_prepare()`
3. The example deinitializes the LCD and enters `esp_light_sleep_start()`
4. The wake timer resumes execution
5. The example reinitializes the panel and calls `esp_lv_adapter_sleep_recover()`

Example log flow:

```text
I (xxx) main: LVGL Sleep Demo
I (xxx) main: LCD resolution 800x480
I (xxx) main: Sleep mode: adapter pause + PM
I (xxx) esp_lvgl:adapter: Auto sleep entered
I (xxx) esp_lvgl:adapter: Auto sleep wake requested
I (xxx) esp_lvgl:adapter: Auto sleep exited
```

For RGB/MIPI targets, the status label also reports wake cause and the last timed sleep duration.

## Minimal Sequence

USER mode, inside the `on_enter_sleep` callback (error handling omitted):

```c
esp_lv_adapter_sleep_prepare();              // adapter: pause + detach + guard
hw_lcd_deinit();                             // app: release the panel
esp_sleep_enable_timer_wakeup(timeout_us);   // app: pick the wake source
esp_light_sleep_start();
hw_lcd_init(&panel, &io, ...);               // app: re-create the panel
esp_lv_adapter_sleep_recover(disp, panel, io); // adapter: rebind + resume
```

PAUSE mode: set `mode = ..._PAUSE` with enter/exit callbacks that blank/restore
the panel; call `esp_lv_adapter_request_wake()` (timer or input) to wake.

## Key APIs Used

- `esp_lv_adapter_request_wake()` / `esp_lv_adapter_request_wake_from_isr()`
- `esp_lv_adapter_sleep_prepare()`
- `esp_lv_adapter_sleep_recover()`
- `esp_light_sleep_start()`
- `esp_lcd_panel_disp_sleep()` / `esp_lcd_panel_disp_on_off()`
- `hw_lcd_deinit()` / `hw_lcd_init()`
- `esp_pm_configure()`

## Troubleshooting

**Display not working after wake-up:**

- Verify the example callback restores panel state correctly
- For RGB/MIPI, ensure reinitialized panel handles are passed back into `esp_lv_adapter_sleep_recover()`
- For SPI/QSPI, check whether the panel driver supports `esp_lcd_panel_disp_sleep()`; the example falls back to `esp_lcd_panel_disp_on_off()`

**Sleep not entering:**

- Verify `CONFIG_PM_ENABLE` and tickless idle are enabled for SPI/QSPI pause mode
- Check whether recurring LVGL timers or animations are keeping `lv_timer_handler()` busy
- For RGB/MIPI, verify the wake timer is configured before `esp_light_sleep_start()`

**UI state lost after sleep:**

- Ensure `esp_lv_adapter_sleep_prepare()` is called before LCD deinit
- Ensure `esp_lv_adapter_sleep_recover()` is called after LCD reinit
- Keep the original `lv_display_t *` handle alive across the full sleep cycle
