# Rotation-in-draw host regressions

This runner builds **this adapter's** `lv_draw_ortho.c` and PPA backend against
LVGL, comparing rendered pixels with LVGL's software renderer. It reuses pinned
MIT-licensed fixtures from the same author's `ppa-alternative-` project instead
of importing the standalone ESPHome component into this adapter.

Requirements: Python 3.9+, Git, make, a C compiler and pthreads on Linux.
Clone the fixtures and LVGL, then run from this directory:

```sh
git clone https://github.com/youkorr/ppa-alternative- /tmp/ppa-fixtures
git clone --branch v9.5.0 --depth 1 https://github.com/lvgl/lvgl /tmp/lvgl
python3 run.py --reference /tmp/ppa-fixtures --lvgl /tmp/lvgl --mutants
```

The runner itself downloads nothing. Fixtures are pinned to
`6cf0eed1277032489fd0281890b9a326c93f063f`; the base scene suite is pinned to
`9db8952fc7f06438819fd4aaf28f5fb8a35a56b7`, before the standalone project's
experimental split/tile-width controls. The newer fake PPA and font regression
are retained. These commits must exist in the reference checkout.

Runs cover single-core and pthread rendering, 90/180/270-degree rotation,
RGB565/XRGB8888, clipping, partial/direct buffers, image-cache hits, eviction,
budget limits and mutable exclusion. The fake PPA defers DMA until waited for,
checks the additional FIFO margin, reproduces the exact-depth stall, and checks
CPU fallback after a timeout. A font callback spy checks concurrent calls;
`lv_deinit()` must release cached pixels.

`--mutants` temporarily removes each of the font guard, FIFO margin, padded-cache
geometry fix and cache cleanup in the **staged test copy** and requires the corresponding regression
to fail. Repository sources remain unchanged. `--sanitize` enables AddressSanitizer
and UndefinedBehaviorSanitizer; fixture-wide leak detection is disabled because
the historical scene suite retains test images, while the explicit cache teardown
assertion remains active. `--build-dir /tmp/ortho-tests` retains the staged build
for inspection and incremental rebuilds (use an empty directory initially).

Host timing and a fake PPA are not hardware performance measurements. Compile
`../lvgl9` with `sdkconfig.defaults;sdkconfig.ci.rotate_in_draw_cache` and run
the `[ortho_cache]` Unity test on the supported ESP32-P4 EV board to check real
flash/PSRAM admission, DMA and adapter teardown. The accompanying pytest case
waits for Unity's final result, including the heap check in `tearDown()`.
