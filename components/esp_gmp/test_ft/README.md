# ESP File Transfer Protocol Tests

## Overview

This test suite validates the wire-format protocol of the `esp_gmp` File Transfer profile.

## Golden Frame Tests

The golden frame tests serve as a **protocol compatibility baseline**. Each test case contains:

1. **Golden frame data** - exact byte sequences that represent valid protocol messages
2. **Encode tests** - verify that encoding produces the expected wire format
3. **Decode tests** - verify that decoding correctly parses golden frames

### Purpose

- **Wire format stability**: Any change that breaks these tests indicates a protocol breaking change
- **Interoperability**: Ensures devices running different firmware versions can communicate
- **Regression prevention**: Catches unintended protocol modifications during refactoring

### Protocol Wire Format

The file transfer protocol uses **Big Endian (network byte order)** for all multi-byte fields:

- `transfer_id`: 32-bit BE
- `file_size`: 64-bit BE
- `block_size`: 32-bit BE
- `block_index`: 32-bit BE
- `reason_code`: 16-bit BE

### Test Coverage

- ✅ META_REQUEST encode/decode
- ✅ META_RESPONSE encode/decode
- ✅ DATA_REQUEST header encode/decode
- ✅ DATA_RESPONSE encode/decode
- ✅ FINAL_CONFIRM encode/decode
- ✅ ABORT encode/decode
- ✅ Round-trip encode/decode validation
- ✅ Truncated/malformed decode rejection (META_REQUEST, ABORT)
- ✅ ABORT reason codes used on link-down / abort paths (`LINK_ERROR`, `ABORTED`)
- ✅ Encode-only happy-path wire sequence (META → DATA → FINAL → ABORT)

Golden frames freeze the on-wire layout for regression during refactors. Full
link-down race against a live FT worker remains a device/integration test TODO
(needs FreeRTOS FT init + GMP mock or hardware).

## Running Tests

```bash
cd components/esp_gmp/test_ft
idf.py build flash monitor
```

## Adding New Tests

When adding new protocol fields or messages:

1. Create a golden frame with known byte values
2. Add encode test comparing output to golden frame
3. Add decode test parsing golden frame
4. Add round-trip test for data preservation

## Protocol Compatibility

**IMPORTANT**: The golden frames define the on-wire contract. Any test failure after code changes means:

- Protocol compatibility may be broken
- Devices with old/new firmware may not interoperate
- Careful version negotiation or migration path is required

Before merging changes that modify golden frames, ensure:
- [ ] Protocol version is incremented if applicable
- [ ] Compatibility with existing deployed devices is considered
- [ ] Migration/fallback strategy is documented
