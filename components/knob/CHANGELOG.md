# ChangeLog

## v0.1.5 - 2024-7-3

###  Enhancements:
* Support power save mode

## v0.1.4 - 2023-11-23

* Fix possible cmake_utilities dependency issue

## v0.1.3 - 2023-6-2

###  Enhancements:

* Add power on knob position detection to avoid logical inversion caused by knob position
* Change test to test_apps project

## v0.1.2 - 2023-3-9

### Enhancements:

* Use cu_pkg_define_version to define the version of this component.

## v0.1.1 - 2023-1-18

### Bug Fixes:

* Knob:
  * Fix callback return usr_data root pointer, the usr_data of the relevant callback will now be returned.

## v0.1.0 - 2023-1-5

### Enhancements:

* Initial version

* The following types of events are supported

|   EVENT    |                  描述                  |
| ---------- | -------------------------------------- |
| KNOB_LEFT  | EVENT: Rotate to the left              |
| KNOB_RIGHT | EVENT: Rotate to the right             |
| KNOB_H_LIM | EVENT: Count reaches maximum limit     |
| KNOB_L_LIM | EVENT: Count reaches the minimum limit |
| KNOB_ZERO  | EVENT: Count back to 0                 |

* Support for defining multiple knobs

* Support binding callback functions for each event and adding user-data