# ESP LV Present

[![Component Registry](https://components.espressif.com/components/espressif/esp_lv_present/badge.svg)](https://components.espressif.com/components/espressif/esp_lv_present)

English | [中文](README_CN.md)

`esp_lv_present` is the **LVGL 9 flush binding** for
[`esp_display_present`](../esp_display_present/README.md). The application owns
the `esp_lcd` panel and the presenter; this component only installs an LVGL
display on that presenter.

It is not a second display stack and not a replacement for
[`esp_lvgl_adapter`](../esp_lvgl_adapter/README.md).

```text
LVGL 9 widgets
      │  flush / invalidation
esp_lv_present          ← this component (LVGL 9 only)
      │  presenter contract (PARTITION / DIRECT / FULL)
esp_display_present     ← buffers, TE, rotation, panel submit
      │
esp_lcd panel
```

## Requirements

- **ESP-IDF**: >= 6.0
- **LVGL**: 9.x only (`>=9,<10`). LVGL 8 is not supported.
- **esp_display_present**: the application must create the presenter first.

## vs `esp_lvgl_adapter`

| | `esp_lv_present` | `esp_lvgl_adapter` |
|---|---|---|
| Role | Bind LVGL to an existing `esp_display_present` presenter | Full LVGL port: display, input, lock, optional FS / decoder / FreeType |
| LVGL | 9.x only | 8.x and 9.x |
| LCD bring-up | Application (`esp_lcd` + presenter) | Application (`esp_lcd`); adapter owns LVGL display buffers |
| LVGL task / lock | None. The task that calls `esp_lv_present_start()` must pump `lv_timer_handler()` | Worker task plus `esp_lv_adapter_lock()` / `unlock()` |
| Touch / encoder | Not included | Included |
| When to use | Presenter-based flush, GSP/LVGL handoff, present-mode benchmarks | Typical LVGL UI that does not use `esp_display_present` |

Do not register the same panel through both components at once. Adapter and
this binding each expect to own the LVGL display and the refresh path.

## Pair with `esp_display_present`

1. Initialize the `esp_lcd` panel (for example with `hw_init`).
2. Fill `esp_display_present_target_config_t` and call
   `esp_display_presenter_create()`. Mode `AUTO` is usually enough; see the
   [presenter README](../esp_display_present/README.md) for contracts and TE.
3. Call `esp_lv_present_start(presenter, &disp)` from the task that will pump
   LVGL. The binding rebinds the presenter producer to that task.
4. Create widgets on `disp` and drive `lv_timer_handler()` on the same task.
5. Before another producer (for example GSP) takes the presenter: quiesce,
   `esp_lv_present_stop()`, then hand off. `stop()` does not delete the
   presenter.

```c
#include "esp_display_present.h"
#include "esp_lv_present.h"

esp_display_presenter_t *presenter = NULL;
ESP_ERROR_CHECK(esp_display_presenter_create(&presenter_config, &presenter));

lv_display_t *disp = NULL;
ESP_ERROR_CHECK(esp_lv_present_start(presenter, &disp));

/* Create the LVGL UI, then pump LVGL from this same task. */
for (;;) {
    uint32_t delay_ms = lv_timer_handler();
    vTaskDelay(pdMS_TO_TICKS(delay_ms > 0 ? delay_ms : 1));
}

ESP_ERROR_CHECK(esp_display_presenter_quiesce(presenter, 2000));
ESP_ERROR_CHECK(esp_lv_present_stop());
ESP_ERROR_CHECK(esp_display_presenter_delete(presenter));
```

Only one binding may be active. The first successful `start()` initializes LVGL
and an `esp_timer` tick.

Contracts:

- **PARTITION**: LVGL borrows the presenter's draw-buffer pool. Finalized LVGL
  invalidation areas are framebuffer-repair coverage.
- **DIRECT / FULL**: LVGL renders into the presenter's framebuffer lease. This
  component does not allocate another full buffer.

## Add the dependency

```
idf.py add-dependency "espressif/esp_lv_present"
```

Or in `idf_component.yml`:

```yml
dependencies:
  espressif/esp_lv_present: "^0.1.1"
```

In this repository:

```yml
dependencies:
  espressif/esp_lv_present:
    override_path: "components/display/tools/esp_lv_present"
```

`esp_display_present` and LVGL 9 are pulled in automatically.

## Example and test app

- Example: `examples/display/gui/lvgl_present_benchmark` — LVGL 9 benchmark on
  an app-owned presenter.
- Example: `examples/display/gui/gsp_lvgl_present_handoff` — GSP ↔ LVGL
  presenter ownership handoff.
- Test app: `test_apps/` — Unity start/stop and flush checks on a software
  stub panel (no LCD). Same shape as `esp_lv_decoder` / `esp_lv_fs`.

```sh
cd components/display/tools/esp_lv_present/test_apps
idf.py set-target esp32c3 build flash monitor
```

## See also

- [`esp_display_present`](../esp_display_present/README.md) — presenter and
  panel submit
- [`esp_lvgl_adapter`](../esp_lvgl_adapter/README.md) — full LVGL adapter
  without this presenter
