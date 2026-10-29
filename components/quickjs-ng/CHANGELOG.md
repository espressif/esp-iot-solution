# ChangeLog

## v0.14.0 - 2026-04-27

- Vendored upstream [quickjs-ng v0.14.0](https://github.com/quickjs-ng/quickjs/tree/v0.14.0) under `quickjs-ng/`, pruned to library sources only.
- Port layer: `port/stub.c` (weak libc symbols), `port/include` (WASI / `poll.h` / `lstat` glue for `quickjs-libc.c` on newlib).
- Example: `examples/quickjs-ng` — `cd examples/quickjs-ng && idf.py set-target <chip> build`.
