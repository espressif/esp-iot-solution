# ChangeLog

## v0.1.1 - 2026-09-14

### Enhancements:

* Read each touch report from `0xD000` in one burst with no address-write delay
* Keep current firmware on the `D101` report page after identification

### Bug Fixes:

* Adapted the HYN212 path for current firmware (Y coordinates and D228 check code)
* Acknowledge every non-legacy report immediately so the controller can publish the next frame
* Keep the coordinates reported by the controller instead of dropping Y values above the debug-mode resolution
* Treat malformed reports as empty frames after acknowledgement

## v0.1.0 - 2026-08-31

### Enhancements:

* Added CST9220 touch controller support
* Added legacy and new firmware compatibility
* Added low-power support

### Bug Fixes:

* Fixed interrupt acknowledgement
* Fixed release records being reported as active points on legacy firmware
