# Transformed-object hardware validation

On 12 November 2026, the repository owner ran the V6 test applications on a
Waveshare ESP32-P4-WIFI6-Touch-LCD-7B and an original M5Stack Tab5. Both use
ESP-IDF 5.5.6, LVGL 9.6.0, RGB565, 360 MHz CPU, 200 MHz PSRAM and 128 KiB L2.

Each application shows an independent software-reference bitmap beside live
LVGL objects: 16 scenes, all four screen rotations and two cycles, 128 steps
per board. The owner confirmed that the two images matched visually on both
boards. The logs establish completion and heap integrity; they do not provide
an automated pixel-equality check of the physical panel.

| Board | Completed steps | Heap checks | After every one of 8 display teardowns | Duration |
|---|---:|---:|---|---:|
| Waveshare 7B, EK79007 1024x600 | 128/128 | 128 passed | 34,080,503 free bytes, 532,643 internal bytes, 6 tasks | 146 s |
| Original Tab5, ILI9881C 720x1280 | 128/128 | 128 passed | 34,075,835 free bytes, 528,367 internal bytes, 6 tasks | 149 s |

There were no crashes, watchdogs or failed heap checks. All eight post-teardown
measurements were identical within each board; no cumulative loss was observed
between those measurement points. A ninth initialization deliberately leaves a
final comparison visible, so eight teardowns are expected.

Both logs end with:

```text
PR818_V6_DONE completed=128 expected=128 heap_ok=1 pixel_equality=VISUAL_CHECK_REQUIRED
```

Immutable test-kit sources, manifests, build settings and firmware checksums:

- [Waveshare V6](https://github.com/youkorr/ppa-alternative-/tree/aadc6345694faa008f38661990464e1f93763163/downloads/pr818-waveshare-v6)
- [Original Tab5 V6](https://github.com/youkorr/ppa-alternative-/tree/b2942199fc584875aaa411756bfaf07c5072936f/downloads/pr818-tab5-v6)

These kits exercise the per-display software-layer policy. The PR additionally
preserves the existing custom-matrix/skew path and upgrades the older FreeRTOS
callback guard; those compatibility cases have separate host regressions.

These are short correctness/stability tests, not endurance tests or video-FPS
benchmarks. `render_us` includes locking, complete widget/reference reconstruction
and refresh; a deliberate one-second pause separates scenes. No new speed-up
percentage is inferred from these logs. Transformed-object layers can require
additional memory and resampling time.
