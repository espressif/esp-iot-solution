# Protobuf files for esp_local_ctrl client

These files mirror IDF `esp_local_ctrl` packet structures:

* `esp_local_ctrl.proto` — get/set property count and values
* `constants.proto` — **not duplicated here**; imported from
  [`protocomm_ext/proto/constants.proto`](../../protocomm_ext/proto/constants.proto)
  (shared `Status` enum / `status__descriptor`)

Generated C sources under `../proto-c` are checked in; regenerating is optional.
`constants.pb-c.*` are provided by the `protocomm_ext` component at link time.

## Compilation

Requires `protoc` and `protoc-c`. From this directory:

```bash
make
```

Or with cmake:

```bash
mkdir build && cd build && cmake .. && make
```
