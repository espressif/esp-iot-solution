# esp_lv_present Test App

Unity tests for the LVGL 9 ↔ `esp_display_present` binding. No LCD hardware
is required: the suite uses a software stub `esp_lcd` panel/IO, same idea as
`esp_lv_fs` / `esp_lv_decoder` (in-memory LVGL display, generic pytest env).

```sh
idf.py set-target esp32c3 build flash monitor
```
