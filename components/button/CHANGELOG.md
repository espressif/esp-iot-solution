# ChangeLog

## v2.5.6 - 2023-8-22

### bugfix

* Fixed a bug where the Serial trigger interval in button_long_press_hold event fires at an incorrect time

## v2.5.5 - 2023-8-3
* Add modify api which can change long_press_time and short_press_time

## v2.5.4 - 2023-7-27

### Enhancements:

* Add test apps and ci auto test

## v2.5.3 - 2023-7-26

### Enhancements:

* `repeat` defined in struct button_dev_t is reset to 0 after event `BUTTON_PRESS_REPEAT_DONE`

## v2.5.2 - 2023-7-18

### Enhancements:

* Set "event" member to BUTTON_PRESS_REPEAT before calling the BUTTON_PRESS_REPEAT callback

## v2.5.1 - 2023-3-14

### Enhancements:

* Update doc and code specification
* Avoid overwriting callback by @franz-ms-muc in #252

## v2.5.0 - 2023-2-1

### Enhancements:

* Support custom button 
* Add BUTTON_PRESS_REPEAT_DONE event