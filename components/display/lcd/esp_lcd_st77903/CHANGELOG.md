# ChangeLog

## v0.3.0 - 2023-09-08

### Enhancements:

* Implement the driver for the ST77903 LCD controller
* Support RGB and QSPI interface

## v0.3.1 - 2023-11-03

### bugfix

* Fix the missing check of flags `auto_del_panel_io` and `mirror_by_cmd` for RGB interface
* Remove LCD command  `29h` from the initialization sequence for QSPI interface
