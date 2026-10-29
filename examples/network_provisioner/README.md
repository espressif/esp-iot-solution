# Network Provisioner Example

Controller example using `network_provisioner` + `protocomm_ext` against a device running
[`espressif/network_provisioning`](https://components.espressif.com/components/espressif/network_provisioning)
(`wifi_prov` / `thread_prov`).

## Modes (compiled together; pick at runtime)

Serial console menu — one or more of (depending on chip / Kconfig):

| # | Mode | Notes |
|---|------|--------|
| 1 | SoftAP (HTTP) + Wi-Fi | STA joins device SoftAP first |
| 2 | BLE + Wi-Fi | needs NimBLE central + observer |
| 3 | BLE + Thread | needs NimBLE (peer needs IEEE 802.15.4) |

In menuconfig you can **independently** enable/disable `EXAMPLE_SUPPORT_SOFTAP_WIFI` /
`EXAMPLE_SUPPORT_BLE_WIFI` / `EXAMPLE_SUPPORT_BLE_THREAD` to shrink the binary; defaults
compile SoftAP + BLE Wi-Fi when Wi-Fi/NimBLE are available.

Security is **auto-detected at runtime** from the peer `proto-ver` JSON (`prov.sec_ver`):
Sec0 needs nothing; Sec1 prompts for PoP; Sec2 prompts for username/password.
Empty Enter uses the Kconfig defaults (`EXAMPLE_PROV_POP` / Sec2 user+pass).

**Test-only defaults:** PoP / Sec2 credentials in Kconfig match the official device examples
for lab bring-up. Do not ship production firmware with fixed secrets.

Sensitive console input (PoP, Sec2 password, Wi-Fi password) is masked with `*`.

## Build

```bash
cd examples/network_provisioner
idf.py set-target esp32
idf.py menuconfig   # SoftAP/BLE prefix, default PoP / Sec2 creds
idf.py build flash monitor
```

On ESP32, Wi-Fi + NimBLE needs IRAM savings (already in `sdkconfig.defaults`). If link
still overflows IRAM, turn off unused `EXAMPLE_SUPPORT_*` or Bluetooth.

## Device side

1. Flash official `wifi_prov` or `thread_prov`.
2. SoftAP: set SoftAP SSID to match the device AP.
3. BLE: set BLE name prefix (often `PROV_`).
4. Align PoP / Sec2 with the device example (test only).
5. Thread: enter Active Operational Dataset hex at the console prompt (optional Kconfig default is lab-only).
6. On the controller monitor, choose mode `1` / `2` / `3` as needed (can run another after).
