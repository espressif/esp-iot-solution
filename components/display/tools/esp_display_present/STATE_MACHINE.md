# Presenter state and ownership model

This document describes internal invariants. It does not add to the public API
contract.

## Producer frame

The serial producer moves through these states:

| State | Entry | Permitted exit | Ownership invariant |
|---|---|---|---|
| `IDLE` | create, cancel, or completed commit call | `begin` | No active frame and no facade lease. |
| `ACTIVE` | successful `begin` | `acquire`, `commit`, or `cancel` | One frame ticket belongs to the bound producer. |
| `REGION_HELD` | successful `acquire` | `submit` or `cancel` | Exactly one lease id and one backend region are live. |
| `SUBMITTED` | successful `submit` | another `acquire`, `commit`, or `cancel` | The facade holds no region; submitted pixels belong to the active frame. |
| `COMMITTED` | endpoint accepts `commit` | asynchronous retire | The facade is idle. Hardware ownership is represented only by the submitted ticket/fence. |
| `CANCELLED` | pre-commit failure or explicit cancel | `IDLE` | Every pre-commit lease and active region has been returned or invalidated. |

`submit` failure performs the same pre-commit cleanup as `cancel`. Repair is the
commit boundary: once repair or endpoint commit has made a buffer visible to
hardware, it must not be returned by generic cancellation. Only completion of
the matching ticket may retire it. Duplicate, stale, or out-of-order completion
must not manufacture a free buffer.

## Draw-buffer copy worker

With two partition draw buffers, an optional worker copies a submitted band
after `submit` while the producer renders the next one. Slot reuse, repair,
commit, cancel and teardown all drain the worker before touching or releasing
its buffers; submitted TE work must be committed or cancelled before stop.

## Ordered framebuffer switches

Ordered PARTIAL commits retire the previous display buffer from the inflight
FIFO when the panel reports frame done. Some RGB drivers fire that event only
when the DMA switches buffers, so a second switch requested before the first
took effect would let one event stand for two switches and keep a skipped
buffer inflight. RGB commits therefore wait until the FIFO is empty before
requesting the next switch.

Frame-done callbacks may arrive during submission. RGB replays an early
completion only after color transfer completes. Pending state, submit readiness
and buffer retirement are updated together before waking the producer.

## Tracked display confirmation

A receipt pairs its frame ticket with the current binding generation. Rotation
rebuilds that binding, clears confirmation, and preserves monotonic frame IDs.
Tracked commits hold the next `begin` within the pipeline acquire budget until
the switch fence and two later RGB/MIPI VSYNCs. A timeout retains hardware
ownership. Cancellation clears only the matching frame ID and generation;
stale/repeated cancellation returns `ESP_ERR_NOT_FOUND`, and late feedback cannot
confirm a cancelled frame.

The switch completion records its VSYNC sequence in the tracker. Delayed status
queries use that event timestamp and do not restart the confirmation wait.

The first tracked commit installs VSYNC callbacks, retained until binding
teardown. Ordinary commits do not enable them or introduce a confirmation hold.
Tracking adds no task or framebuffer.

Status readers join `active_calls` only in `OPEN` or `FAULTED`. They acquire the
lifecycle lock before the tracker lock so the pending ticket and event counters
form one snapshot. ISR updates acquire only the tracker lock; an older counter
snapshot cannot confirm a ticket installed concurrently.

## TE phase wakeups

On IDF 6.2+, a TE context may allocate one one-shot `esp_timer` on the shared
timer task. It only wakes the producer; the TE timestamp and phase window still
control transfer admission. Allocation/arm failure keeps the edge/tick fallback.
Teardown waits for the timer callback before releasing its semaphore or context.
Panel and GPIO callback removal also drain already-entered ISRs on other cores
before freeing their contexts.

## Lifecycle

| State | Meaning | Invariant |
|---|---|---|
| `OPEN` | Producer calls may enter. | Callback context and endpoint storage are live. |
| `CLOSING` | Stop has excluded new calls. | A held facade tile must be returned before endpoints that require it can stop. Callbacks remain attached while hardware can reference presenter state. |
| `STOPPED` | Endpoint stopped and callbacks detached. | No ISR can enter presenter-owned state; deletion is safe. |
| `FAULTED` | Terminal transfer/protocol failure. | The instance never reopens. Hardware-accepted storage is retained unless the endpoint can prove callback ownership is gone. |

The `active_calls` counter protects the `OPEN` to `CLOSING` edge. Stop and
delete are not producer operations and must never silently cancel another
task's lease. Handoff therefore occurs only between frames, after quiesce, and
advances from the latest submitted or completed ticket so a callback from the
old producer cannot retire a new producer's frame.

## Test seam

The component test app supplies an in-memory `esp_lcd` panel and wraps RGB
callback registration at link time. This seam is private to the test binary:
the public headers and target configuration stay unchanged, while begin,
acquire, submit, repair, commit, completion, and quiesce still execute the
production presenter, endpoint, mode, transform, and tracker code.
