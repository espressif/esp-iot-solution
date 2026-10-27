# Changelog

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
