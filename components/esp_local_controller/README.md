# ESP Local Controller

Controller-side client for IDF [`esp_local_ctrl`](https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-reference/protocols/esp_local_ctrl.html).

| Component | Role |
|-----------|------|
| **esp_local_ctrl** | Device **server** (properties) |
| **protocomm_ext** | Controller **pipe** (transport + Sec0/1/2) |
| **esp_local_controller** (this) | Controller **business** (`control` protobuf) |

## Capability matrix

| Transport | Sec0 | Sec1 | Sec2 |
|-----------|:----:|:----:|:----:|
| HTTP | ✓ | ✓ | ✓ |
| BLE (NimBLE central) | ✓ | ✓ | ✓ |
| UART Console | ✓ (debug) | ✓ | ✓ |

## Typical flow

Runtime security discovery (recommended):

```c
#include "esp_local_controller.h"
#include "protocomm_ext.h"
#include "protocomm_ext_security.h"

protocomm_ext_t *pc = protocomm_ext_new(&pe_cfg); /* transport + Sec0 placeholder */
esp_local_controller_t *ctrl = esp_local_controller_create(pc);

esp_local_controller_fetch_version(ctrl, /* BLE: &ble_addr, else NULL */ NULL);
esp_local_controller_version_t ver = {0};
esp_local_controller_get_version(ctrl, &ver);
/* protocomm_ext_set_security(pc, ...) from ver.sec_ver + PoP / Sec2 creds */
esp_local_controller_version_free(&ver);

esp_local_controller_establish_security(ctrl);

uint32_t count = 0;
esp_local_controller_get_property_count(ctrl, &count);
/* get / set property values by index */

esp_local_controller_stop_session(ctrl);
esp_local_controller_delete(ctrl);
protocomm_ext_delete(pc);
```

If the peer security scheme is **already known**, call `protocomm_ext_set_security()`
first, then `esp_local_controller_start_session()` (fetch_version + establish_security).

**Thread safety:** APIs on one instance are not thread-safe; serialize from a single task.

## Dependency

This component requires **`protocomm_ext`** from the same `esp-iot-solution` tree
(`components/protocomm_ext`, via `override_path` in `idf_component.yml`).

```yaml
dependencies:
  espressif/esp_local_controller: "^1.0.0"
```

When both components are published to the Component Registry, the `protocomm_ext`
version constraint in this component’s manifest resolves without a local override.

## Tests

```bash
cd components/esp_local_controller/test_apps
idf.py set-target esp32 build
```

## Example

See [`examples/esp_local_controller`](../../examples/esp_local_controller). Pair with the
official IDF example `examples/protocols/esp_local_ctrl`.

**Supported targets:** ESP32 / ESP32-S3 / ESP32-C2 / ESP32-C3 / ESP32-C5 / ESP32-C6 / ESP32-C61 / ESP32-H2 / ESP32-P4

## License

Apache-2.0. See `license.txt`.
