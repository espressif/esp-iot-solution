# ESP Local Controller Example

Controller example using `esp_local_controller` + `protocomm_ext` against a device running
IDF [`esp_local_ctrl`](https://github.com/espressif/esp-idf/tree/master/examples/protocols/esp_local_ctrl).

## Modes (compiled together; pick at runtime)

| # | Mode | Notes |
|---|------|--------|
| 1 | HTTP | STA joins lab Wi-Fi, then POSTs to device URL. After a successful session, keep it open and connect another device; up to 4 HTTP sessions stay up together. Entering a URL that is already held resumes that session |
| 2 | BLE | NimBLE central, observer, and GATT client; pick peer from scan. One BLE connection |
| 3 | Console | Optional; needs a console protocomm peer (debug) |
| 4 | HTTP resume | Talk to a session that is already open, without a new handshake |
| 5 | HTTP close | Close every held HTTP session |

Security is **auto-detected** from the peer version JSON (`local_ctrl.sec_ver`):
Sec0 needs nothing; Sec1 prompts for PoP; Sec2 prompts for username/password.
Empty Enter uses Kconfig defaults (`EXAMPLE_CTRL_POP` / Sec2 user+pass).

**Test-only defaults** match the official IDF `esp_local_ctrl` example (PoP / Sec2
`wifiprov`/`abcd1234`). Do not ship production firmware with fixed secrets.

## Build

```bash
cd examples/esp_local_controller
idf.py set-target esp32c3
idf.py menuconfig   # Wi-Fi SSID/password, HTTP URL, PoP / Sec2
idf.py build flash monitor
```

## Device side

1. Flash IDF `examples/protocols/esp_local_ctrl` (HTTP or BLE, Sec0/1/2).
2. HTTP: put both devices on the same AP; set `EXAMPLE_HTTP_URL` to the device IP.
3. BLE: device name defaults to `my_esp_ctrl_device`; filter via `EXAMPLE_BLE_NAME_PREFIX`.
4. Align PoP / Sec2 with the device menuconfig.
5. HTTP multi-session: finish device A and accept "Keep this HTTP session open", then pick HTTP again for device B. Menu 4 switches back to A without disconnecting B. Entering A's URL again resumes A. One device still accepts only one controller, so the menu keeps a single slot per device. A held device is matched by scheme, host, and port (`http` is port 80, `https` is 443). Write an IPv6 address in brackets, for example `http://[2001:db8::1]`.
