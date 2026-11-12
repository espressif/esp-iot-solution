# Development and validation

Production components are `esp_display_present` and sibling `esp_lv_present`.
See [modes](../docs/MODE_ARCHITECTURE.md) and [state/lock domains](../STATE_MACHINE.md).

## Host regression

CMake, a C/C++ compiler and Python 3 are required. No hardware or download:

```sh
python3 dev/tools/dev.py test
python3 dev/tools/dev.py test --sanitize
```

Binding tests require existing LVGL 9 source:

```sh
python3 dev/tools/dev.py test --lvgl-dir /path/to/lvgl
python3 dev/tools/dev.py test --lvgl-dir /path/to/lvgl --sanitize
```

Use the actual CTest count for each configuration. Non-LVGL tests do not validate
the LVGL binding. Build output defaults to `/tmp/esp-display-present-tests`.

## Board tests

`boards/boards.json` and `boards/configs/` define boards and defaults.
`test_apps/` keeps reusable pixel, benchmark, TE lifecycle, RGB handoff and smoke
programs. Activate the matching ESP-IDF (>=6.0) before preparing IDF projects.

```sh
python3 dev/tools/run.py list
python3 dev/tools/run.py --help
```

Select the board, port and action explicitly for hardware operations. Use
`--work-root` for persistent output and Flash backups. Temporary driver observers
are isolated diagnostics, never clean-driver performance evidence.

## Offline report

[The five-board report](evidence/display-latest-20261111/README.md) records tested
versions from 2026-11-11. Later code edits are not automatically board-tested.

```sh
python3 dev/tools/build_latest_dashboard.py --output /tmp/display-latest/matrix-data.json
python3 dev/tools/render_comparison.py --data /tmp/display-latest/matrix-data.json --output /tmp/display-latest/comparison.html
```

The final snapshot contains scene, memory and handoff observations; historical
logs, build folders and boards are unnecessary for rendering. Raw provenance
and experiments are listed in [the archive index](ARCHIVE.md).

Board driver dependencies are prepared from the configured local component cache
into the external work directory by `run.py prepare`; dependency source copies
are not part of this candidate. Standalone board projects can use
`boards/prepare_deps.py --help` and pass `PRESENT_DEPS_DIR` to CMake.
The historical standalone legacy-compat preparation scripts are excluded; the
maintained Host facade/resources regressions remain in `tests/`.
