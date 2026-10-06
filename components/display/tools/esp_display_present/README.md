# ESP Display Present

[![Component Registry](https://components.espressif.com/components/espressif/esp_display_present/badge.svg)](https://components.espressif.com/components/espressif/esp_display_present)

English | [中文](README_CN.md)

`esp_display_present` connects a renderer-produced pixel surface to an
`esp_lcd` panel without depending on scene, widget, or input semantics. It
abstracts the pixel path (buffers, TE, rotation, submit) so the same presenter
can serve LVGL, GSP, or a custom GUI.

It is not [`esp_lvgl_adapter`](../esp_lvgl_adapter/README.md). The adapter is a
full LVGL port (display, task, lock, input). This component does not know LVGL.
For LVGL 9, bind flush with [`esp_lv_present`](../esp_lv_present/README.md).

```text
LVGL 9 / GSP / other GUI
        │  pixels + damage
esp_display_present     ← this component
        │
esp_lcd panel
```

## Add to Project

```
idf.py add-dependency "espressif/esp_display_present"
```

Or in `idf_component.yml`:

```yml
dependencies:
  espressif/esp_display_present: "*"
```

## Responsibilities

- Validate the display target and select a presentation strategy.
- Lease render surfaces or bounded draw buffers.
- Submit full-frame or dirty-area updates.
- Handle framebuffer ownership, TE synchronization, rotation, byte order,
  cache synchronization, and optional hardware copies.
- Track ordered completion and shut down without releasing in-flight buffers.

The renderer owns logical damage and pixel generation. The presenter owns
physical buffer coherence and panel submission.

## Public API

Include `esp_display_present.h`. Configuration and shared types are defined in
`esp_display_present_config.h` and `esp_display_present_types.h`.

The primary object is `esp_display_presenter_t`. A renderer:

1. creates a presenter from an `esp_display_present_target_config_t`;
2. acquires a frame contract;
3. renders the requested full surface or partition;
4. submits logical coverage, or cancels the frame;
5. stops and deletes the presenter after producer work has drained.

The draw contract is one of:

| Contract | Renderer responsibility |
|---|---|
| `PARTITION` | Render requested areas into a presenter-owned draw buffer |
| `DIRECT` | Render dirty areas into a coherent full logical surface |
| `FULL` | Redraw the complete logical surface |

Presentation mode should normally remain `ESP_DISPLAY_PRESENT_MODE_AUTO`.
Accurate panel class, framebuffer, TE, rotation, and byte-order information in
the target configuration lets the component select the correct path.

## Panel initialization

Before creating the panel, set its explicit type, mode, rotation and TE
configuration, then query the minimum number of panel framebuffers:

```c
uint8_t panel_fb_count;
ESP_ERROR_CHECK(esp_display_present_get_required_frame_buffer_count(
    &target, &panel_fb_count));
```

Use this count for the RGB/MIPI driver's `num_fbs`, then fill `target.hw.panel`
and `target.hw.io` and create the presenter with the same target configuration.
Keep `target.fb.frame_buffer_count = 0` when using driver-owned buffers; that
field counts supplied framebuffer addresses, not the panel's allocation.
IO/GRAM returns 0 because draw buffers and TE compose buffers are separate.
The query does not access handles or allocate memory. Unsupported static policy
combinations are rejected; actual driver and buffer checks happen at creation.
For a runtime mode matrix, allocate the maximum requirement of its supported
cases.

## Presentation modes

| Mode | Target and buffers | Producer contract | Rotation and validation |
|---|---|---|---|
| `NONE` | GRAM panel: no framebuffer; RGB/MIPI: one framebuffer | `PARTITION` on GRAM, `DIRECT` on RGB/MIPI | GRAM supports 0° only. Host-tested; not selectable in the current test apps. |
| `DOUBLE_FULL` | RGB/MIPI, two framebuffers | `FULL`; redraw every frame | Rotation is implemented through a logical transform surface. Host-tested; not selectable in the current test apps. |
| `TRIPLE_FULL` | RGB/MIPI, three framebuffers | `FULL`; redraw every frame | All rotations are included in the runtime benchmark matrix. |
| `DOUBLE_DIRECT` | RGB/MIPI, two framebuffers | `DIRECT`; dirty rendering at 0° | Non-zero rotation is rejected. |
| `DOUBLE_PARTIAL` | RGB/MIPI, two framebuffers plus partition draw buffers | `PARTITION`; repairs stale framebuffer regions | Rotation is fused into partition copies and included in the runtime benchmark matrix. |
| `TRIPLE_PARTIAL` | RGB/MIPI, three framebuffers plus partition draw buffers | `PARTITION`; repairs stale framebuffer regions | All rotations are included in the runtime benchmark matrix; the default P4 handoff path also resolves to this mode. |
| `TE_SYNC` | SPI/I80/QSPI GRAM panel; one or two full-frame compose buffers | `PARTITION`; pushes a full frame at TE | Rotation is fused into compose-buffer placement and included in the runtime matrix on compatible GRAM targets. |
| `AUTO` | Resolves from panel type and TE configuration | Uses the resolved mode's contract | GRAM resolves to `TE_SYNC` when TE is enabled, otherwise `NONE`; RGB/MIPI resolves to `TRIPLE_PARTIAL`. |

The validation labels above describe coverage present in this repository, not
a guarantee for every panel driver or board.

`DOUBLE_DIRECT` is defined only at 0°. Use a FULL mode for complete-frame
rotation or a PARTIAL mode for rotated dirty rendering.

## Partition draw buffers

PARTIAL and `TE_SYNC` use bounded draw buffers (about 32 KiB by default).
With two buffers, an optional worker copies the previous band, including the
RGB565 byte swap, while the producer renders the next band. With
`drawbuf.buffers = 0`, GRAM without TE defaults to two; FB repair and `TE_SYNC`
use two only when memory permits, while single-core targets keep one. Set
`drawbuf.buffers = 1` to retain the single-buffer path. Worker allocation
failures fall back to synchronous copies.

On RGB panels, a framebuffer switch is requested only after the previous one
has taken effect, because some RGB drivers report completion only when their
DMA switches buffers. The third framebuffer keeps rendering meanwhile, so
producers faster than the refresh rate still reach it.

## SPI/QSPI configuration for TE mode

`ESP_DISPLAY_PRESENT_MODE_TE_SYNC` overlaps rendering with an in-flight
full-frame transfer, but it does not make a blocking panel driver asynchronous.
Configure the SPI/QSPI panel IO so `esp_lcd_panel_draw_bitmap()` can enqueue one
complete frame and return before the transfer finishes:

- Set the SPI bus `max_transfer_sz` to at least one physical frame when the
  target and driver support it: `width * height * bytes_per_pixel`.
- Set panel IO `trans_queue_depth` to at least the number of transactions the
  driver uses for one frame. Reserve one additional entry if that driver also
  queues a command transaction. For a driver payload limit of `chunk_bytes`,
  use `ceil(frame_bytes / chunk_bytes)` data entries. For example, a 480 × 480
  RGB565 frame is 460800 bytes; with 32 KiB payloads it takes 15 data chunks,
  so a queue depth of 16 leaves one entry for a command.
- Enable direct PSRAM DMA when both the SoC and the selected ESP-IDF panel-IO
  driver provide that capability. Some targets expose this through a
  driver-specific flag such as `psram_dma_direct`; do not set such a flag on
  drivers that do not define it. Compose buffers are allocated with DMA
  capability; without direct PSRAM DMA, the driver may copy through internal
  memory or split the transfer more aggressively.
- Ensure the panel initialization enables its TE output (commonly the `TEON`
  command), and configure the real TE GPIO, bus frequency, and data-line count
  in `te_sync`.

The transfer-complete callback must fire once the queued frame is actually
finished. If `draw_bitmap()` takes approximately the full wire-transfer time,
the transaction queue is too shallow (or the driver is synchronous), and the
render/transfer pipeline collapses to serial execution. Increasing compose
buffers cannot fix that condition; correct the bus/driver configuration first.

Before Display Off or panel teardown, stop frame production and call
`esp_display_presenter_quiesce()`. Keep the panel IO and its completion ISR
alive until quiesce succeeds, then turn the panel off and delete the presenter.

## Concurrency

The presenter has one frame producer. Another task may request stop, but frame
acquire, submit, and cancel calls must remain serialized. In-flight hardware
work may temporarily make deletion return `ESP_ERR_INVALID_STATE`; retain the
object and retry after the producer has stopped. Commit or cancel submitted
work before stopping; stop does not take ownership of a producer's frame.

Headers under `private_include/` and files under `src/` are implementation
details and carry no public compatibility promise.

The internal frame/lifecycle transition table and buffer ownership invariants
are recorded in [`STATE_MACHINE.md`](STATE_MACHINE.md).

## GUI backends

This component only presents pixels. It is not tied to one GUI:

| | `esp_display_present` | `esp_lvgl_adapter` |
|---|---|---|
| Role | Send renderer pixels to `esp_lcd` | Full LVGL port (display, task, lock, input) |
| GUI | Any: LVGL, GSP, custom | LVGL 8/9 only |
| LVGL | No dependency. Use `esp_lv_present` for LVGL 9 | Owns the LVGL refresh path |

See [`esp_lv_present`](../esp_lv_present/README.md) for the LVGL 9 binding.

## Example and test app

- Example: `examples/display/gui/lvgl_present_benchmark` — LVGL 9 benchmark on
  an app-owned presenter.
- Example: `examples/display/gui/gsp_lvgl_present_handoff` — GSP ↔ LVGL
  presenter ownership handoff (needs LCD + `espressif/esp-gsp`).
- Test app: `test_apps/` — Unity create/delete and PARTITION frame submit on a
  software stub panel (no LCD). Same shape as `esp_lv_decoder` / `esp_lv_fs`.

```sh
cd components/display/tools/esp_display_present/test_apps
idf.py set-target esp32c3 build flash monitor
```
