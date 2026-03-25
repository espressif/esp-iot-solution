# ESP Weaver Test Application

This directory contains unit tests for the ESP Weaver component.

## Test Coverage

| Category | Tests | What it verifies |
|----------|-------|------------------|
| `[weaver][init]` | Node init/deinit, singleton enforcement | Core lifecycle and state transitions |
| `[weaver][device]` | Create/delete, duplicate rejection, add/remove from node | Device lifecycle and constraints |
| `[weaver][param]` | All value types, UI/bounds, update, type mismatch | Parameter CRUD and type safety |
| `[weaver][device][param]` | Lookup by name/type, duplicate rejection, primary param | Device-parameter integration |
| `[weaver][integration]` | Full workflow, set_params callback, change tracking | End-to-end data flow |
| `[weaver][memory]` | String param lifecycle, full workflow loop | Memory leak detection |

## Building and Running

```bash
cd components/esp_weaver/test_apps
idf.py set-target esp32
idf.py build flash monitor
```

When prompted, type `*` to run all tests, or enter a tag (e.g., `[init]`) to filter.
