# EAF component tests

Run these applications from a source checkout. The component registry package
excludes `test_apps/` and the Host port.

| Application | Runs as | Coverage | Display |
|---|---|---|---|
| `core` | Assertions at boot, no menu | EAF/EAF4 loading, BGRA pixels and dirty rectangle, delta seek, concurrent JPEG/JPEG Delta players, software fallback or rejection | None; no LVGL dependency |
| `lvgl8` | Unity menu | LVGL 8 loading, flush, configuration, repeat/restart, pause/resume and failed source replacement | Simulated 240×240 |
| `lvgl9` | Unity menu | Same adapter checks with LVGL 9 | Simulated 240×240 |
| `host` | `make test` / `make sanitize` | Production C/Python roundtrip, all frame modes, bounds, corrupt input, seeking, concurrent streams and CLI contracts | None |

## Requirements and fixtures

Activate ESP-IDF 5.5 or newer before using `idf.py`. The first device build may
fetch component dependencies from the registry: zlib, software JPEG, and LVGL
for the two adapter applications. Device fixture generation needs only the
SDK's Python standard library, not NumPy, Pillow or FFmpeg.

`generate_test_assets.py` creates files under each build's `eaf_assets/` directory:

| File | Used by | Content |
|---|---|---|
| `legacy.eaf` | All device apps | One 16×16 Palette RLE frame |
| `lossless.eaf4` | All device apps | Two 16×16 BGRA8888 frames, 67 ms period: FULL plus a packed XOR update of one 4×4 tile |
| `jpeg.eaf` | `core` | 32×16 JPEG keyframe plus a JPEG Delta partial update |
| `unaligned.eaf` | `core` | 16×16 JPEG that needs software fallback on supported hardware JPEG targets |
| `direct.eaf` | `core` | 48×16 JPEG suitable for direct hardware RGB565 output |

All fixture data is generated or embedded in this repository. No external image
files or asset downloads are needed.

## Core on ESP32-S3

From this `test_apps/` directory:

```sh
cd core
idf.py -B build-s3 -DIDF_TARGET=esp32s3 -DSDKCONFIG=sdkconfig.s3 build
idf.py -B build-s3 -p /dev/ttyUSB0 flash monitor
```

Replace the serial port with the attached board's port. Tests run once at boot.
Success ends with `GUI-independent EAF core test passed`; there must be no failed
assertion, abort or panic. Reset the board to repeat the test.

## Hardware JPEG on ESP32-P4

Run both configurations from `core/`, using separate build directories and
sdkconfig files so switching configurations does not reuse stale options:

```sh
# Hardware JPEG with software fallback enabled (default)
idf.py -B build-p4-sw -DIDF_TARGET=esp32p4 -DSDKCONFIG=sdkconfig.p4.sw build
idf.py -B build-p4-sw -p /dev/ttyACM0 flash monitor

# Hardware JPEG with software fallback disabled
idf.py -B build-p4-hw -DIDF_TARGET=esp32p4 -DSDKCONFIG=sdkconfig.p4.hw \
  '-DSDKCONFIG_DEFAULTS=sdkconfig.defaults;sdkconfig.ci.no_sw_jpeg' build
idf.py -B build-p4-hw -p /dev/ttyACM0 flash monitor
```

The supplied P4 defaults select 16 MB flash and HEX PSRAM at 200 MHz. Adjust
these settings in `menuconfig` for the actual board before flashing.

Both runs must reach the core success message. JPEG Delta must preserve pixels
outside the changed region; repeated open/seek/close and concurrent players must
complete. The unsupported-width fixture must decode with software enabled and
be rejected with software disabled. That negative case can print JPEG decode
errors; the expected rejection is asserted and the run must still reach the core
success message. On targets without hardware JPEG, disabling
software must reject JPEG while EAF/EAF4 non-JPEG checks continue to pass.

For ESP32-S31, use an SDK that supports that target; the recorded device runs used
an ESP-IDF 6.2 development checkout. Select `esp32s31` with distinct build and
sdkconfig paths; add `--preview` if required by that SDK. No physical LCD is
needed for the core tests.

## LVGL adapters on ESP32-S3

From this `test_apps/` directory:

```sh
cd lvgl8                         # use lvgl9 for LVGL 9
idf.py set-target esp32s3
idf.py build
idf.py -p /dev/ttyUSB0 flash monitor
```

Select “run all tests” from the Unity menu. Require zero failures, including:

- EAF and EAF4 sources load and the simulated display receives a flush;
- the two-frame animation advances, remains stationary while paused, continues
  after resume, and returns to frame zero on restart;
- the EAF4 period is used unless explicitly overridden;
- repeat configuration survives playback and restart, and failed source replacement
  leaves the object unloaded;
- the Unity leak checks stay within the 700-byte threshold for both tracked heaps.

These are simulated display tests. They do not initialize a panel or establish
physical screen appearance, DMA correctness or tear-free playback.

## Host checks

Run from the component root (one directory above `test_apps/`):

```sh
python3 -m pip install -r requirements.txt
make -C test_apps/host test
make -C test_apps/host sanitize
python3 tools/render_codec_docs.py --check
```

Host prerequisites and validation boundaries are in [TESTING.md](../docs/TESTING.md).

## Continuous all-mode conformance

The optional core configuration checks all codec IDs 0x00..0x06 and EAF4 frame
modes 0..11 using 49 procedural cases. It compares every decoded pixel against
source references, checks bounded error at 0/4/8, interleaves two players, seeks
forward/backward, checks cache release, and exercises invalid codecs and CRCs.
JPEG references allow a 16-level RGB tolerance for backend rounding; unsupported
hardware widths must fall back with software enabled or fail with it disabled.

Install `requirements.txt` into the Python used for fixture generation. Unlike
the default five core fixtures, this optional generator uses NumPy and Pillow:

```sh
# From the component root, with the ESP-IDF environment active
python -m pip install -r requirements.txt
cd test_apps/core
idf.py -B build-all -DIDF_TARGET=esp32s3 -DSDKCONFIG=sdkconfig.all \
  '-DSDKCONFIG_DEFAULTS=sdkconfig.defaults;sdkconfig.ci.all_modes' build
idf.py -B build-all -p /dev/ttyUSB0 flash monitor
```

If the SDK Python has no encoder dependencies, pass
`-DEAF_TEST_FIXTURE_PYTHON=/absolute/path/to/python` pointing to an environment
where the component requirements are installed. Fixture generation imports the
production encoder and needs no Host library or prebuilt test assets.

For P4/S31, select that target and the SDK/board settings described above. To test
without software JPEG, append `;sdkconfig.ci.no_sw_jpeg` to the quoted defaults
list and use separate build and sdkconfig paths. JPEG rejection cases are expected
in that configuration, including all JPEG inputs on S3.

The firmware prints each case on the first sweep, followed by
`ROUND PASS ... cases=49`. It repeats continuously, checking heap stability after
each sweep. A failed comparison asserts/aborts. Keep several successful rounds;
there must be no restart, panic or growing heap loss. These are serial pixel
checks; this configuration does not drive an LCD.
