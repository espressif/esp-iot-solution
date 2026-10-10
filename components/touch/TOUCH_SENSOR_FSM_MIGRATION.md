# Touch sensor FSM migration

The touch button sensor, slider sensor, and proximity sensor wrappers in this
repository now use the v1.0 complete-frame APIs from `touch_sensor_fsm` and
`touch_sensor_lowlevel`. This is a breaking interface change. Rebuild every
application that includes one of these wrappers; the old fields and frequency
specific FSM handles are intentionally not kept as compatibility aliases.

## Configuration fields

| Wrapper | Replace | With |
| --- | --- | --- |
| `touch_button_config_t` | `channel_threshold`, `channel_gold_value`, `debounce_times` | `thresholds`, `sample_period_ms`, `sample_cfg_num`, `debounce_active`, `debounce_inactive` |
| `touch_slider_config_t` | `channel_threshold`, `channel_gold_value`, `debounce_times` | `thresholds`, `sample_period_ms`, `sample_cfg_num`, `debounce_active`, `debounce_inactive` |
| `touch_proxi_config_t` | `channel_threshold`, `channel_gold_value`, `debounce_times` | `thresholds`, `sample_period_ms`, `sample_cfg_num`, `proximity_count`, `debounce_active`, `debounce_inactive` |

`thresholds` are unsigned absolute values in the wrapper's normalized fused
FSM decision domain. They are not float ratios, and they are not physical raw
counts. Tune them from the target's returned amplitude after startup. Zero
sampling fields select the low-level defaults; zero debounce fields select FSM
defaults.

## Data and callbacks

The wrappers now submit one complete frequency frame with
`touch_sensor_fsm_update_data()`. Frequency-specific FSM handles and the old
`FSM_DATA_*` selectors must be removed from callers. Button, slider, and
proximity `get_data()` return the positive part of the fused filtered
amplitude. The underlying FSM `touch_sensor_fsm_get_data(..., int32_t *data)`
returns signed `fused_filtered`; these wrappers explicitly clamp negative
values to zero before converting to their unsigned amplitude type. Direct FSM
callers must change their output variable to `int32_t` and rebuild. Slider `touch_slider_sensor_get_data()` no longer accepts a
frequency argument. Proximity remains an independent accumulated-count
low-level type and does not use ordinary touch 5000 alignment or the ordinary
touch frequency estimator; its effective FSM period includes the actual
`proximity_count` reported by low-level.

Applications that need ordinary touch frequency diagnostics should call
`touch_sensor_lowlevel_get_frequency()` after a complete scan is available.
The API returns estimates in Hz; `div_num` and `charge_times` remain
separate sampling parameters. An unavailable estimate must be reported as an
error rather than displayed as zero.

## Diagnostic header visibility

The published `touch_button_sensor` component has no debug switch, private
noise API, or dependency on `touch_sensor_fsm_private.h`. It uses only the
public FSM interface, so it cannot select `TOUCH_SENSOR_FSM_DEBUG` or consume
FSM snapshots from a published package. For raw/baseline/FSM diagnostics,
build the repository's `examples/fsm` application; that application owns the
debug option and explicitly adds the FSM private include path. The button
wrapper's public build must remain independent of diagnostic types and
symbols.

## Published component dependencies

The wrappers and the button example resolve the released components through
the component manager:

- `espressif/touch_sensor_lowlevel` `^1.0.2`
- `espressif/touch_sensor_fsm` `^0.9.0`
- `espressif/touch_button_sensor` `^0.3.0`

No repository-local `override_path` should point to a sibling `fsm_refactor`
checkout. The published button component intentionally exposes only its public
API; use the FSM repository example when private diagnostics are required.

## Thresholds and validation limits

`thresholds` now maps directly to FSM `threshold_active`; the wrappers use
the default release threshold of `threshold_active / 2` (integer division).
The FSM no longer has a `hysteresis` configuration field. EMI observation is
disabled by default (`emi_enable = false`), independently of diagnostic logging.
For example, a wrapper threshold of 70 now presses above 70 (plus noise
compensation) and releases below 35; the previous default comparisons were
above 80 and below 60. Recheck application tuning after this change.
The default positive limiter is 1.5 times the base threshold for one frequency and three
times the threshold for multiple frequencies. The limiter must be strictly
above the active threshold; otherwise initialization reports
`ESP_ERR_INVALID_SIZE`. The examples use reachable starting values, not an
exact conversion from the old float ratios. Tune them against measured idle
noise and touches on each target. Slider position uses the limited fused
amplitudes, so large saturated touches can reduce position resolution.

Proximity frame timing uses the configured accumulation count, not the task's
polling interval. A reused proximity instance reports its actual count through
low-level metadata. Physical slider gestures and ESP32-S3 proximity behavior
still require board validation; passing source/build tests is not evidence of
those hardware behaviors.
