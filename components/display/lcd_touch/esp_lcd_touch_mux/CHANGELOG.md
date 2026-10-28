# ChangeLog

## v0.1.0 - 2026-09-25

### Enhancements:

* Add a multiplexer for physical and injected touch input
* Support queued gestures with explicit release and cancellation
* Wake interrupt consumers through a one-shot GPTimer alarm
* Support polling consumers without an interrupt callback
* Consume at most one injected frame per read to preserve frame order
