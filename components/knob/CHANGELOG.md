# ChangeLog

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