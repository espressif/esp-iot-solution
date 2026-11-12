# State machines

The component uses one presentation engine. See the historical [flows (archive)](dev/ARCHIVE.md) and [ownership sequences (archive)](dev/ARCHIVE.md); the current transition and validation status is linked from [migration status](MIGRATION_STATUS.md). The earlier implementation is available in Git history (`41b0c9b9b`).


## GRAM cancellation, deadlines and retirement

These invariants describe the current implementation; they do not add a new
public API or change scheduling or release conditions.

- A producer lease grants write access. Successful submission consumes that
  lease; accepted copy/transfer work retains the actual source independently
  of the public lease identity and the frame observation record.
- Exhausting the acquire/admission budget is retryable while no physical
  transfer has expired. Expiring an accepted physical transfer records a
  sticky fault. Cancellation and a close request are not transfer completion.
- Cancel revokes the logical build and old lease identities. Pending copies or
  transfers still own their sources; a repeated cancel may need another bounded
  drain attempt. An outstanding producer lease cannot be confiscated by STOP.
- Source retirement requires a genuine matching completion or successful
  adapter quiescence proving that hardware access and callbacks have ended.
  A timeout or failed STOP/delete cannot establish that proof. The real IO
  adapter retains its pending transaction and callback registration while a
  completion is missing. There is no forced-free path for a missing callback.
- A late completion may establish resource safety and allow explicit teardown;
  it does not clear the first fault or turn a failed frame into a success.
  Finishing one band cannot release another band's outstanding source.

The public facade serializes acquire and STOP with its mutex. It has no legacy
`active_calls` counter. Successful deletion still requires the caller to exclude
concurrent API calls; testing STOP against acquire does not authorize racing
successful deletion against a live caller.

## Service and lock domains

This section records who may advance state; it changes no scheduling.

One engine instance has one `gate` mutex. It serializes the engine's
task-side callers, listed below; it is not the only lock protecting
backend state. Backend ISR event handlers run under the backend's own
lock (`present_lock_enter(&backend->lock, isr)`), not the gate: for
example the REGION ISR path calls `complete_tx()` directly, mutating
transaction and completion state in ISR context. That ISR-side mutation
is immediate, not deferred until the gate is next held; the gate only
serializes when task-side callers observe and act on the results. Do not
reason about backend state as if the gate alone made it single-threaded.

- The owner task, through the owner-checked engine APIs (`service`,
  `begin`/`acquire`/`submit`/`commit`/`cancel`, `query`/`wait`/`release`,
  `drain`, `stop`, `set_rotation`). `esp_display_present_engine_service()`
  and the background worker are the only callers of the backend `service`
  op (via `pump_backend`); the other owner calls reap completions and
  refresh the fault under the same gate, and reaping may invoke the
  backend `query`/`release_receipt` ops, which are not guaranteed free of
  service side effects (the FB release path advances its pipeline first).
- The per-instance background backend worker, when the backend runs in
  background mode. Each pass takes the gate, runs one `service` pass plus
  transfer-deadline expiry, then releases the gate and wakes the owner.
  For a background REGION backend it is also the only context allowed to
  enter the panel driver submit; foreground calls that reach a ready slot
  signal the worker instead.
  While the worker is inside the driver submit it lends the gate back,
  with the submitting slots pinned; backend task state is only resolved
  after the gate is reacquired.
- The owner-neutral `present_presenter_control_*` bypass used by the
  facade/resources layer. These take the gate without an owner check, so
  their callers must serialize against producer calls themselves (the
  facade mutex and the resources create/stop sequencing do this).

Gate handling while blocked depends on the wait, and the two cases must
not be conflated. `wait_change` (used by `wait_frame` and drain) releases
the gate, waits on the instance `changed` event, and reacquires before
inspecting state, so that kind of waiting owner cannot starve the
worker. STOP is different: `present_presenter_control_stop` takes the
gate and holds it across `stop_impl`, whose backend `stop` may block
waiting for the pixel worker to go idle and for panel quiesce. During
that window every other gate user is excluded, and any callback or
reentrancy analysis must assume the gate is held, not lent. ISR-context
panel/TE events do not take the gate; they update the ISR-safe trackers
and, as above, mutate backend state under the backend's own lock.

The pixel-copy worker is a separate, narrower domain: one serial worker
per pixel engine instance (not a global queue shared across instances).
It dispatches queued copies strictly by ascending token, one at a time.
A queued copy therefore waits for every copy ahead of it and for the
completion callback each of those copies invokes synchronously on the
worker (`finish_copy` runs the notification callback before the next
copy starts). No hard bound on that wait is established: it grows with
queue depth, copy sizes and callback duration. Synchronous pixel
engines run the copy on the submitting caller instead. There is no
evidence requiring a different policy; do not add priorities or extra
workers without measurements showing this blocking matters.

## State authority and backend op side effects

Authority is divided by duty, not layered so that an upper level
overrides a lower one. Each level below is authoritative only inside
its own scope, and specifically cannot decide the things listed.

| Scope | Authoritative for | Must not decide |
|---|---|---|
| Backend-private state (one instance per presenter; includes ISR-side changes made under the backend's own lock) | REGION draw/TX slots, FB candidate/scanout buffers, TE compose/send stages | That hardware has stopped merely because work was logically cancelled |
| `present_frame_core` (one instance per backend, not a shared global) | Frame records, pending/leased bookkeeping, terminal flags, the first fault | That a scanout buffer is reusable: frame terminal does not imply the panel has finished reading it |
| Engine completion tracker | Receipts awaiting observation, terminal snapshots, counters, error projection | Pixel-buffer reuse: a cached terminal snapshot grants no ownership |
| Facade | Public generation, compatibility state, the success watermark | Hardware completion: the watermark is a publication view updated from engine/backend results; it judges nothing independently and grants no buffer reuse |

The common backend logic already lives in `present_frame_core`. What
remains in each backend is a thin op wrapper, and the wrappers differ
in side effects, not just in lines:

| Op | Actual per-backend behavior | Disposition |
|---|---|---|
| `get_fault` | All three are the same one-line delegation to `present_frame_core_get_fault` | Keep the thin wrappers |
| `query` | All three run their backend `service` first, then the core query; FB passes `supports_present=true`, REGION/TE pass `false` | Keep each backend's advance step |
| `wait` | All three share `present_frame_core_wait`, parameterized by their own query callback and the `supports_present` flag | Already shared; nothing further to extract |
| `release_receipt` | FB and TE run `service` first; REGION does not service before releasing | Must not be unified; the ordering difference is observable |
| `get_caps` | Layout, capabilities and buffer policy are computed from different per-backend state | Keep per-backend implementations |
| `begin`/`cancel`/`stop`/`destroy` | Lifecycle and resource recovery genuinely differ (draw/TX drain, candidate/scanout refs, composer/timers/sender) | Not touched |

On `query` discarding the `service` return value: that only means this
call's own return code does not carry the service error. There are
three distinct error channels — the function's return code, the
`result` recorded in a frame status, and the sticky fault — and no
promise that any given service error reaches the latter two; whether
it does depends on each backend's `service` implementation.

Further extraction from here would need an added indirection layer,
and there is currently no benefit that justifies one. Keeping these
thin wrappers and the ops boundary is a deliberate design, not
unfinished deduplication. This closes the backend-shell work: do not
reopen it for line count alone. If a future change does extract an op,
acceptance requires host tests that actually cover error codes,
service-call ordering, early completions and receipt release across
the affected backends, with the full host suite passing in the
configuration under test (baseline recorded 2026-11-12: 44/44 in the
default non-LVGL configuration). A passing total by itself does not
substitute for those behavior checks.

## LCD IO completion ordering (SPI/QSPI)

The LCD IO adapter maps an untagged `on_color_trans_done` to the oldest
outstanding cookie. This is a transport precondition, not a way to detect
lost, duplicated or foreign callbacks. Its scope is one exclusively owned
IO handle, serialized submit calls, and a panel driver whose successful
`draw_bitmap` issues exactly one `esp_lcd_panel_io_tx_color`. It does not
establish ordering across IO handles, nor support a panel that emits one
color transfer per row without aggregating completion itself.

Source audit (2026-11-12):

| ESP-IDF revision | Basis | Audited implementation |
| --- | --- | --- |
| `7b9cc1ac79f865983f59bb8ff3ff43eb74ff1dbe` | Revision recorded in `dev/evidence/s31-adapter-te-baselines-20261107/manifest.json` | LCD SPI IO and SPI master queue/ISR |
| `80c8922480b5aad5038a61d38f6b3d0de5727602` | Local `esp-idf-gitlab` HEAD; the two audited files have no working-tree edits | Same completion-ordering mechanism |

In both revisions, `components/esp_lcd/spi/esp_lcd_panel_io_spi.c`:

- `panel_io_spi_tx_color` acquires the SPI device's bus, splits large color
  payloads into queued transactions, clears each descriptor before reuse,
  and sets `en_trans_done_cb` only on the final chunk.
- `lcd_spi_post_trans_color_cb` calls `on_color_trans_done` only for that
  marked chunk, with no transaction identifier (`event_data` is NULL).
  Command transfers do not set that flag. SPI and QSPI share this code;
  quad mode changes the wire flags, not the completion scheme.

In `components/esp_driver_spi/src/gpspi/spi_master.c`,
`spi_device_queue_trans` appends to the device's FreeRTOS queue with
`xQueueSend`. The ISR takes transactions with `xQueueReceiveFromISR`,
finishes the current transaction and runs `spi_post_trans`/`post_cb` before
starting the next. This establishes FIFO completion for this device;
arbitration with other devices can delay it, but does not reorder its queue.
These are implementation findings for the listed revisions, not a blanket
promise for other transports or future/custom drivers.

There are two distinct levels of splitting:

1. **Driver chunks:** one successful `tx_color` produces one color-done
   callback after its last SPI chunk. The source must remain valid until
   that callback, even if `draw_bitmap` has already returned.
2. **Present staging bands:** creation with staging forces adapter capacity
   to one transaction. `stage_transfer` reuses one scratch buffer, submits one band and
   waits for that band's real color-done before copying the next band.
   `finish_color` signals intermediate bands without popping the cookie;
   only the final band's completion publishes logical success. A pending
   band's timeout does not prove DMA stopped: storage stays pinned until
   real completion. If submission fails without a final callback, quiesce
   may time out while retaining storage; it must not fabricate completion.

The adapter also permits completion before `draw_bitmap` returns: it queues
the cookie before entering the panel driver. Callback removal is not a DMA
drain; quiesce first waits for outstanding cookies, active callbacks and the
submit call itself to retire. Host `test_lcd_io.c` covers FIFO cookies, early
completion, staged delayed reads and timeout retention with a fake panel;
it does not verify the hardware driver's ordering. Recheck both the panel's
one-color-transfer contract and the IDF queue/callback path when changing
drivers or supported revisions. No extra detection or scheduling is added.

## Fault classification at the facade

The backend's first accepted-operation error is the authoritative original
error. The facade's `fault_reason` is only a projection of it, and
`esp_display_presenter_get_fault_error()` returns the original value before
classification. The first fault is latched; later observation neither clears
nor reclassifies it.

| Original error | Public `fault_reason` |
|---|---|
| none (`ESP_OK`) | `ESP_DISPLAY_PRESENT_FAULT_NONE` |
| `ESP_ERR_TIMEOUT` | `ESP_DISPLAY_PRESENT_FAULT_TRANSFER_TIMEOUT` |
| any other accepted-operation error (`ESP_FAIL`, `ESP_ERR_NO_MEM`, `ESP_ERR_NOT_SUPPORTED`, `ESP_ERR_INVALID_STATE`, `ESP_ERR_INVALID_ARG`, ...) | `ESP_DISPLAY_PRESENT_FAULT_TRANSFER_FAILED` |

`ESP_DISPLAY_PRESENT_FAULT_PRODUCER_PROTOCOL` never comes from this
projection. It is latched only by the facade's own protocol checks (currently
a non-timeout failure while finishing a producer cancellation). Direct API
misuse returns its error to the caller without latching a fault. This is an
observable classification contract: a non-timeout transfer failure must not
be reported as a timeout, because the two suggest different recovery.

Host regressions in `dev/tests/test_legacy_facade.c` cover one/two exhausted
GRAM tiles, independent admission/transfer deadlines, and STOP reaching the
facade mutex after acquire enters its event wait. Test-only linker wrappers
establish that ordering with condition variables. They do not change firmware.
`dev/tests/test_transfer_timeout.c` uses a controlled clock to exercise DONE
at deadline minus one microsecond, exactly at deadline, and plus one microsecond,
with task observation before/after DONE and cancellation/failed STOP before DONE.
It also covers cancellation after only one of two transfers completes. Tests
check source contents, invalidated leases, unchanged submission counts, retained
callbacks, sticky results and successful explicit retirement after real DONE.

A missing callback is held back through repeated failed teardown attempts and
then supplied to clean up the fixture. This verifies retention during the test;
it is not an infinite-duration test, a hard driver-call deadline, a target-IDF
compatibility test, or proof of physical DMA/scanout behavior.
