# Rotation-in-draw host regressions

This runner builds **this adapter's** `lv_draw_ortho.c` and PPA backend against
LVGL, comparing rendered pixels with LVGL's software renderer. It reuses pinned
MIT-licensed fixtures from the same author's `ppa-alternative-` project instead
of importing the standalone ESPHome component into this adapter.

Requirements: Python 3.9+, Git, make, a C compiler, pthreads and zlib development
headers (`-lz`) on Linux. The Python runner uses only the standard library.
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

The OS-mutex cleanup uses the same patch helper as the IDF CMake integration.
In the threaded suite, linker wrappers count successful mutex creation/deletion
in the actual LVGL library. Teardown must balance those counts; removing the
global-mutex release must fail that check. A recognized upstream OS cleanup is
left unchanged, and the inserted code is conditional on the adapter's build
definition so a reused managed component retains upstream behavior when disabled.

`check_freertos_shutdown.py --lvgl /path/to/lvgl` compiles the actual patched
FreeRTOS delete/callback bodies with a deterministic scheduler model. It forces
the remote callback to suspend itself after deletion was queued for idle cleanup,
which can move its TCB out of the termination list. Owner suspension plus a wait
for remote execution to stop must prevent the orphan. The old deletion path and
a wait-skipping mutation must fail. Tests cover ordinary/caps-backed deletion,
the stack-caps hook applied before or after the shutdown hook, disabled guards,
single/multiple cores, delayed eviction and invalid handles. These tests validate
the specific lifecycle interleaving; they are not a full FreeRTOS emulator.

The 1280x720 RGB565 cache test simulates a finite 460 ms whole-image transfer
under the unchanged 250 ms no-progress deadline. Cache preparation must finish
in operations of at most 128 KiB, keep making progress and match every reference
pixel at 90/180/270 degrees. This checks transaction sizing and timeout handling,
not physical PPA timing. A firmware stall report also prints the live completion
count so a slow operation completing later can be distinguished from one that
still has not completed.

`--mutants` temporarily removes each of the font guard, FIFO margin, padded-cache
geometry fix, bounded cache preparation, cache cleanup and OS-mutex cleanup in the **staged test copy** and requires the corresponding regression
to fail. Repository sources remain unchanged. `--sanitize` enables AddressSanitizer
and UndefinedBehaviorSanitizer; fixture-wide leak detection is disabled because
the historical scene suite retains test images, while the explicit cache teardown
assertion remains active. `--build-dir /tmp/ortho-tests` retains the staged build
for inspection and incremental rebuilds (use an empty directory initially).

## Object transforms composed with screen rotation

The same command also runs `transforms.py` and `transform_scenes.c`. The reference
is a separately rebuilt LVGL library with `LV_DRAW_TRANSFORM_USE_MATRIX=0`:
LVGL renders object transforms through its normal bitmap layers, then the final
image is rotated on the CPU. Comparing two matrix-enabled renderers would be an
invalid oracle: both could silently ignore the same unsupported object matrix.

Sixteen scenes cover image-widget rotation/scale, object-style rotation/nonuniform
scale, transformed parents, nested objects, opacity layers and clipping. Each runs
at screen rotations 0/90/180/270, RGB565/XRGB8888 and DIRECT/PARTIAL, with CPU,
pthread and fake-PPA backends: 768 comparisons per LVGL version. All libraries are
rebuilt completely because the per-display policy adds a private display field.

On LVGL 9.6, all 384 DIRECT comparisons must be RGB-exact. LVGL 9.5 DIRECT
scale cases and named PARTIAL cases have explicit small sampling bounds: native
5/6/5-bit channel values are compared for RGB565, not misleading expanded 8-bit
values. Exact comparisons and bounded residuals are reported separately. Missing
frames, crashes and pixels outside a named bound fail the suite.

Additional controls check the default-disabled policy, two-display isolation,
NULL/default selection and display recreation. Custom integer translations and
quarter-turn matrices are compared with the policy off/on and independently
checked against analytic color bounding boxes. A disabled-policy control must
reproduce the lost object transformation; its PARTIAL behavior is recorded per
LVGL version. Fake-PPA transaction counts must prove that backend actually ran.

These tests cover object rotation/scaling and preservation of existing custom
orthogonal matrices. General affine/projective custom matrices and skew are not
new software capabilities. Bitmap layers cost memory and resampling work.

See [hardware validation](HARDWARE_VALIDATION.md) for the two board runs and
downloadable test images. Those short correctness/stability runs do not measure
a new performance gain.

Host timing and a fake PPA are not hardware performance measurements. Compile
`../lvgl9` with `sdkconfig.defaults;sdkconfig.ci.rotate_in_draw_cache` and run
the `[ortho_cache]` Unity test on the supported ESP32-P4 EV board to check real
flash/PSRAM admission, DMA and adapter teardown. The accompanying pytest case
waits for Unity's final result, including the heap check in `tearDown()`.
