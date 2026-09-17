
# ChangeLog

## v0.2.1 - 2026.09.11

### Bug Fixes:

- Handle L2CAP CoC flow control while sending an object. A stalled SDU is queued by the stack and completed when the peer grants credits, but `esp_ble_otp_client_send_data()` reported the stall to the caller as a failure, which aborted the transfer on the very first chunk. The stalled chunk is now reported as sent once `TX_UNSTALLED` arrives, so the sender keeps pacing chunks.
- Transfer contexts are no longer keyed by a connection handle of 0 meaning "free slot". Handle 0 is the first connection a device accepts, so on a peripheral every context lookup failed: the server never recorded the direction or object size of an incoming OACP Write and silently dropped every received SDU while still granting L2CAP credits, letting the client stream a whole object into nothing and report success.
- A disconnect now clears only the context of the link that dropped, instead of every context.
- Client OLCP/OACP requests now publish their pending state before writing the control point. The indication carrying the result is handled on another task and can land first, so assigning afterwards reset the selection back to `SELECTING` (making the following `write_object` fail with `ESP_ERR_INVALID_STATE`) or replaced a started transfer context with a pending one.

- Client auto-discover now uses the `DISC_COMPLETE` connection handle. `esp_ble_conn_get_conn_handle()` returns the first ACL (often a phone at handle 0), so OTS was never discovered on the ESL Tag (handle 1). `discover_ots(0)` was also rejected as invalid even though 0 is a legal connection handle.

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
