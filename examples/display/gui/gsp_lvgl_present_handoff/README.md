| Supported Targets | ESP32-P4 | ESP32-S3 | ESP32-S31 | ESP32-C3 |
| ----------------- | -------- | -------- | --------- | -------- |

# GSP ↔ LVGL Presenter Ownership Handoff

On-target example of GSP ↔ LVGL display handoff over a shared
`esp_display_present` presenter. The app owns the presenter; GSP borrows
it (`esp_gsp_esp_lcd_config_t.presenter`) and LVGL binds through
`esp_lv_present` during its exclusive phase.

Each cycle runs fully automatically:

1. **GSP phase** — the animated hello_world scene runs for
   `CONFIG_APP_PHASE_SECONDS` (perf_log prints fps every 5 s).
2. `esp_gsp_esp_lcd_pause(gsp, 1000, &pause)` quiesces the GSP render
   task and the presenter transfer/present fences.
3. **LVGL phase** — `esp_lv_present_start()` creates an LVGL display on
   the same presenter (producer lease rebound to the app task). A demo
   screen ("LVGL exclusive" title, animated bar, cycle counter) is pumped
   with `lv_timer_handler()` for `CONFIG_APP_PHASE_SECONDS`.
4. `esp_display_presenter_quiesce()` retires the last LVGL frame fences
   before `esp_lv_present_stop()`. If quiesce fails, LVGL remains the active
   producer and continues pumping during a bounded retry window. Exhausting
   the retry budget reports `FAIL` and aborts instead of hanging forever.
5. `esp_gsp_esp_lcd_resume_paused()` resumes GSP; its first frame is a
   forced full redraw.
6. Per-cycle stats are logged:
   `handoff batch=N cycle=k/K pause=Xms lvgl_frames=Y resume=Zms`.

`CONFIG_APP_HANDOFF_CYCLES` (default 5) is one reporting batch. The app repeats
batches until reset. Recoverable failures are counted and logged. Quiesce,
LVGL stop, and GSP resume all have bounded retry budgets. GSP is never resumed
unless LVGL has stopped successfully, so two producers cannot own the presenter
after a failed transition.

## Build / flash

The panel is selected by the `Hardware Configuration` Kconfig choice (ESP32-P4
defaults to the EK79007 1024x600 MIPI-DSI panel). LCD bring-up reuses
`examples/display/gui/common/hw_init`. Scene JSON lives in `scenes/`.
`scenes/DejaVuSans.ttf` is vendored with the example (DejaVu license next to
it). Requires ESP-IDF >= 6.1, `espressif/esp-gsp`,
`espressif/esp_lv_present`, and the GSPC host tool:

```sh
python -m pip install -U esp-gsp-tools
```

CI builds ESP32-S3 / S31 / P4 / C3.

```sh
idf.py set-target esp32p4 build flash monitor
```

## Expected serial output

```
I (xxx) gsp_lvgl_handoff: GSP started on app-owned presenter 1024x600
I (xxx) gsp_lvgl_handoff: batch 1 cycle 1/5: GSP phase (5 s)
...
I (xxx) esp_lv_present: LVGL display 1024x600 on presenter (PARTITION, RGB565, ...)
I (xxx) gsp_lvgl_handoff: PASS batch=1 cycle=1/5 pause=12ms lvgl_frames=140 resume=8ms cycle_errors=0 total_errors=0
...
I (xxx) gsp_lvgl_handoff: batch 1 complete; continuing (total_errors=0)
```

`lvgl_frames` must be above zero each cycle (the animated bar keeps
invalidating); zero frames produce `FAIL`. Pause/resume times should stay
within the 1000 ms budget. An unrecoverable ownership transition logs
`FAIL ... stage=<quiesce|lvgl_stop|resume>` before aborting.
