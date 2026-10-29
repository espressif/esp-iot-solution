# ESP LCD Touch Mux

`esp_lcd_touch_mux` wraps a physical `esp_lcd_touch_handle_t` and presents one compatible handle that can replay injected touch frames or forward physical input.

When the consumer registers an interrupt callback through `esp_lcd_touch_register_interrupt_callback()`, the mux invokes it from a one-shot GPTimer alarm. Without an interrupt callback, the mux operates in polling mode and advances one queued frame on each `esp_lcd_touch_read_data()` call.

Injected coordinates use the same logical coordinate space returned by `esp_lcd_touch_get_data()` on the wrapped physical handle. The mux does not depend on LVGL, GSP, a display service, a fixed resolution, or a fixed GPIO.

Only one injected session may be active. A physical press prevents an injected session from starting, and physical reports are isolated while injection is active. Always finish with `esp_lcd_touch_mux_end()` or recover with `esp_lcd_touch_mux_cancel()`.

Playback is consumer-paced: each read consumes at most one frame. Consumers with different read periods preserve frame order but can take different amounts of time to replay the same queue.

The caller controls ownership of the physical handle through `own_physical_handle`. Hardware validation of GPTimer wake delivery is required per target.

## Integration

Create the physical touch driver first, wrap its handle before registering an interrupt callback, and pass only the returned mux handle to every consumer. Register the existing interrupt callback on the mux handle through `esp_lcd_touch_register_interrupt_callback()` or `esp_lcd_touch_register_interrupt_callback_with_data()`.

```c
esp_lcd_touch_mux_config_t config = ESP_LCD_TOUCH_MUX_CONFIG_DEFAULT();
esp_lcd_touch_handle_t touch = NULL;
ESP_ERROR_CHECK(esp_lcd_touch_mux_new(physical_touch, &config, &touch));
```

An injected gesture is a `begin`, one or more complete frames, and `end`. `end` appends the release frame. Use `cancel` on every playback error path.
