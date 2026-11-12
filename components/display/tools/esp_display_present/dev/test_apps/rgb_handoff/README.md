# RGB handoff regression

This suite runs the real public presenter and scanout adapter against clean IDF.
It is separate from driver-only `rgb-events` and from LVGL performance scoring.

- S31 Korvo stock RGB: no bounce; protocol resolved from BSP creation facts.
- S3 R16 RGB: ten-line bounce; the same public presenter configuration.
- Six framebuffer modes × four angles × producer core 0/1 × submit-return delay
  0/50 ms = 96 cases per board, four full-coverage frames per case. NONE is excluded.
- Each presenter closes and reopens against the same three-buffer panel. The
  initial FB0 fence is exercised before producer leases; mode policy chooses
  its required two/three-buffer subset. DIRECT/PARTIAL ownership and rotation
  follow the real backend, with one input tile.
- A test-only draw wrapper optionally waits 50 ms **after** the real driver
  call and **before** returning to scanout. A callback wrapper forwards the
  original callback and records whether it arrived inside that window.
- All physical output pixels and full-panel bounds are checked after quiesce.
  A delayed case without observed early events is rejected. Capture/report
  requires all 96 distinct cases, no hardware errors, and the suite end marker.

The delay is deliberate fault-window widening, not a measured normal latency.
Pixel-memory verification does not observe LCD scanning and cannot certify the
absence of visible tearing. Visual confirmation remains separate. This suite
provides no FPS or adapter comparison result.

Activate the clean IDF 6.2 environment, then use the normal offline runner from
this component directory. Build products and backups remain outside the repo:

```sh
python dev/tools/run.py prepare --work-root /tmp/present-rgb-handoff
python dev/tools/run.py build --board s31-rgb --suite rgb-handoff --work-root /tmp/present-rgb-handoff
python dev/tools/run.py backup --board s31-rgb --port /dev/ttyUSB0 --work-root /tmp/present-rgb-handoff
python dev/tools/run.py flash --board s31-rgb --suite rgb-handoff --port /dev/ttyUSB0 --work-root /tmp/present-rgb-handoff
python dev/tools/run.py capture --board s31-rgb --suite rgb-handoff --port /dev/ttyUSB0 --timeout 300 --work-root /tmp/present-rgb-handoff
python dev/tools/run.py report --board s31-rgb --suite rgb-handoff --work-root /tmp/present-rgb-handoff
python dev/tools/run.py restore --board s31-rgb --port /dev/ttyUSB0 --work-root /tmp/present-rgb-handoff
```

Ports are examples: verify the board MAC first. Use `--board s3-rgb` with the S3
port for the bounce regression. Restore uses independent full-flash verification;
allow at least ten minutes for the outer automation deadline on large UART images.

The test BSP calls `board_rgb_created()` with the actual successful LCD creation
configuration and forgets it on destruction. Its private
`present_lcd_rgb_get_board_info()` binding serves only the owned live panel;
the application does not choose a protocol. Standard driver queries, if present,
still take precedence. This binding is specific to the maintained single-panel
BSPs; no universal auto-detection from arbitrary opaque handles is claimed.
