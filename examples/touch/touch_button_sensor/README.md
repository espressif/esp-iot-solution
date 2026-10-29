# Touch Button Example

This example uses `touch_button_sensor` to configure one to four touch channels,
report debounced press/release events, and periodically print the public touch
amplitude. A single WS2812 on the configured data GPIO provides immediate
feedback: ESP32-S31 touch IO12 (touch-channel ID 6) is red, IO13
(touch-channel ID 7) is blue, and touching both produces magenta.

Every 10 seconds, `STATS` reports uptime, total press/release counts, free/minimum
heap, and per-channel cumulative counts, presses since the previous report,
current state, current hold time, and longest hold time. Counts reset on reboot.

## Build and Run

```bash
cd examples/touch/touch_button_sensor
idf.py set-target esp32p4
idf.py menuconfig
idf.py build flash monitor
```

ESP32-S31 is available with ESP-IDF 6.1 in the current preview environment:

```bash
idf.py --preview set-target esp32s31
idf.py build flash monitor --port /dev/ttyACM0
```

The default ESP32-S31 wiring is touch IO12 and IO13, WS2812 data on GPIO15, and
an absolute threshold of 500. On ESP32-S31, IO12 maps to touch-channel ID 6 and
IO13 maps to touch-channel ID 7; configure the channel IDs, not the GPIO
numbers. Since lowlevel startup alignment targets about 5000, this threshold is
equivalent to 10% of that reference. Use `Touch Button Example` in menuconfig
to select the channel count (1–4), channel IDs, shared threshold, WS2812 GPIO,
scan period, and sample-configuration count. Only the selected number of
channel entries is compiled into the example.

The threshold is an absolute touch-amplitude value. Tune it using the periodic
`amplitude` output: it should remain comfortably above idle variation and
below the value produced by a normal touch. On hardware, lowlevel `start()`
automatically aligns the first configured channel to approximately 5000 before
sampling; keep that electrode untouched and the board stable while startup
setup runs. A sample-configuration count of 0 uses the lowlevel component's
target-specific default.

The FSM's automatic limiter is 1.5 times the threshold for one frequency and
3 times the threshold for multiple frequencies, not 2 times. ESP32-S31 uses
three sample configurations by default, so the 500 threshold gives an
effective limiter of 1500 normalized counts.

ESP-IDF 5.5 and 6.0 cover ESP32-S2, ESP32-S3, ESP32-P4, and Linux. ESP-IDF
6.1 adds ESP32-H4 and ESP32-S31.

### Sampling frequency configuration

In `idf.py menuconfig`, use **Touch Button Example > Sample configuration count**
(`0` uses the target default; S2/S3 support one configuration). For V3 targets,
choose the built-in Low/High profile under **Component config > Touch sensor
lowlevel**. Alternatively, enable **Touch Button Example > Customize V3 frequency
tuning** to override each frequency's capacitance, resistance and driver settings.
The high-frequency preset requires the RTC fast XTAL clock on supported targets.

The touch-button example intentionally does not print lowlevel register,
frequency-estimate, raw-frame or FSM snapshot data. Use the repository
lowlevel or FSM example when those lower-layer diagnostics are required; the
published button API exposes only the amplitude, state and event behavior used
here.

The touch-button example does not emit a frequency estimate or raw input
diagnostic. Use the repository lowlevel or FSM example for those lower-layer
measurements.

### Live visualization

The example-owned live plot reads only the public button records printed by
this application: amplitude samples and stable press/release callbacks. It
does not read FSM private snapshots, raw samples, baselines, or scan timing.
Those diagnostics belong to the repository FSM example; use
examples/fsm/tools/live_fsm_capture.py after flashing the FSM diagnostic
firmware when internal signal processing must be inspected.

Install the host-side dependencies and run it against a monitor log or directly
against the serial port:

    python3 -m pip install matplotlib pyserial
    python3 tools/live_touch_button_capture.py --port /dev/ttyACM0 --channels 6 7 \
        --record touch_button_sensor.log
    python3 tools/live_touch_button_capture.py --file touch_button_sensor.log --png touch_button_sensor.png

The default serial speed is 115200 baud. Close idf.py monitor before opening
the port from the plotter.
