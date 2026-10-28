# Changelog

## [1.1.0] - 2026-10-27

### Added
- Several HTTP peers can stay connected at once: one `protocomm_ext` instance is one device, with its own client, cookie, TCP keepalive, and security state
- Loopback test: two HTTP peers complete Sec1 and an encrypted request with separate cookies; closing one leaves the other usable

### Changed
- Sec0 uses a per-instance handle, so two plaintext sessions no longer alias each other
- NimBLE central transport is compiled only when `CONFIG_BT_NIMBLE_ROLE_CENTRAL`, `CONFIG_BT_NIMBLE_ROLE_OBSERVER`, and `CONFIG_BT_NIMBLE_GATT_CLIENT` are set, so an HTTP-only controller does not link `ble_gattc_*`

## [1.0.1] - 2026-10-16

### Added
- Target: esp32p4
- `protocomm_ext_get_transport_method()` reports the transport selected at create time

## [1.0.0] - 2026-07-14

### Added
- Client-side protocomm pipe: session lifecycle, Sec0 / Sec1 / Sec2
- Security 2 client (SRP-6a NG_3072/SHA512 + AES-256-GCM)
- Transports: HTTP(S), NimBLE central, UART console
- Runtime `protocomm_ext_set_security()` for deferred scheme selection
- Targets: esp32, esp32s3, esp32c2, esp32c3, esp32c5, esp32c6, esp32c61
- Host unit / console pipe tests; bilingual README; Apache-2.0 license
