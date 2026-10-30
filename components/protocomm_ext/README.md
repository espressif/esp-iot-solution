# Protocomm Extension

Client-side extension of ESP-IDF [`protocomm`](https://docs.espressif.com/projects/esp-idf/en/latest/api-reference/provisioning/protocomm.html).

| Component | Role |
|-----------|------|
| **protocomm** (ESP-IDF) | Device (**server**): accepts provisioning / local-control sessions |
| **protocomm_ext** (this) | Controller (**client**): actively opens sessions, runs security handshake, and exchanges encrypted payloads |

`protocomm_ext` is a **protocol pipe only**. It does **not** encode Wi-Fi provisioning or `esp_local_ctrl` business protobufs. Higher-level components should call `protocomm_ext_send_data()` with already marshalled endpoint payloads (e.g. `prov-config`, `esp_local_ctrl/control`).

## Features

| | Security 0 | Security 1 | Security 2 |
|--|:----------:|:----------:|:----------:|
| HTTP / HTTPS | ✓ | ✓ | ✓ |
| BLE (NimBLE Central) | ✓ | ✓ | ✓ |
| UART Console | ✓ | ✓ | ✓ |

- **Security 0**: plaintext session
- **Security 1**: Curve25519 + AES-256-CTR (optional Proof-of-Possession)
- **Security 2**: SRP-6a (NG_3072 / SHA512) + AES-256-GCM (username / password)
- Kconfig options to strip unused security versions

**Supported targets:** ESP32 / ESP32-S3 / ESP32-C2 / ESP32-C3 / ESP32-C5 / ESP32-C6 / ESP32-C61 / ESP32-P4

## Typical flow

```c
#include "protocomm_ext.h"
#include "protocomm_ext_http.h"
#include "protocomm_ext_security.h"

esp_http_client_config_t http_cfg = {
    .url = "http://192.168.4.1:80",
    .timeout_ms = 5000,
};

protocomm_ext_security1_params_t pop = {
    .data = (const uint8_t *)"abcd1234",
    .len = 8,
};

protocomm_ext_config_data_t cfg = {
    .transport_method = PROTOCOMM_EXT_TRANSPORT_METHOD_HTTP,
    .security_method = PROTOCOMM_EXT_SECURITY_METHOD_SECURITY_1,
    .transport_data = &http_cfg,
    .security_data = &pop,
};

protocomm_ext_t *pc = protocomm_ext_new(&cfg);
protocomm_ext_open_session(pc, NULL);

uint8_t *ver = NULL;
size_t ver_len = 0;
protocomm_ext_get_version_capabilities(pc, "proto-ver", &ver, &ver_len);
free(ver);

protocomm_ext_establish_security(pc, "prov-session");

/* Application owns request / response bytes for business endpoints */
uint8_t *resp = NULL;
size_t resp_len = 0;
protocomm_ext_send_data(pc, "prov-config", req, req_len, &resp, &resp_len);
free(resp);

protocomm_ext_close_session(pc);
protocomm_ext_delete(pc);
```

### Transport notes

- **HTTP**: pass `esp_http_client_config_t *` as `transport_data`. Each instance keeps its own connection, TCP keepalive, and session cookie. Create one instance per device to talk to several peers without disconnecting the others. A device accepts one HTTP client; a second client to that same device replaces the first.
- **BLE**: enable `CONFIG_BT_NIMBLE_ENABLED` plus the central role, the observer role, and GATT client. Use `esp_protocomm_ext_nimble_start_scan()` / `open_session(pc, &ble_addr)`. BLE is one connection, not a session pool. With GATT client off, the HTTP and console transports still build.
- **Console**: pass `protocomm_ext_console_config_t *`. Speaks the same UART text protocol as IDF `protocomm_console` (`<ep> <sid> <hex>\r`).

### Security notes

- Sec1: `protocomm_ext_security1_params_t` (PoP); may be `NULL` if peer expects no PoP.
- Sec2: `protocomm_ext_security2_params_t` with username/password matching the device salt/verifier provisioning.

## Add dependency

```yaml
dependencies:
  espressif/protocomm_ext: "^1.1.0"
```

## Tests

```bash
cd components/protocomm_ext/test_apps
idf.py set-target esp32
idf.py build
pytest --target esp32
```

## Relationship to future components

Provisioning / local-control **client** business layers are intentionally out of scope and will live in separate components on top of this pipe.

## License

Apache-2.0. See `license.txt`.
