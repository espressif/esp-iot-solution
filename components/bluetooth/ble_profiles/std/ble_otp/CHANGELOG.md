## v0.2.0 - 2026.07.28

Enhancements:
- Truncate mode accepts Offset+Length within Allocated Size.
- Include `conn_handle` in OACP/OLCP response event payloads.
- Client GATT read/write/subscribe go to the given `conn_handle`

## v0.1.0

Features:
- Add Object Transfer Profile (OTP) client/server implementation.
- Support OACP operations: Create/Delete/Read/Write/Execute/Abort/Checksum.
- Support OLCP discovery flows and directory listing helper.
- Add write modes: overwrite, append, truncate, patch.
- Add resume helpers (checksum and current-size methods).

Documentation:
- Expand OTP README with procedures, rules, and API matrix.

Examples:
- Add OTP client/server examples with Read + Write + Delete flow.
