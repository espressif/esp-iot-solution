# ChangeLog

## v0.2.1 - 2023-11-23

* Fix possible cmake_utilities dependency issue

## v0.2.0 - 2023-10-9

* Improve recovery mechanism
* Add new model AIR780E
* Fix concurrency issue in AT command mode
* Fix memory stamp issue in `common_get_operator_after_mode_format`


## v0.1.6 - 2023-04-17

* Fix PPP mode can not exit gracefully
* Fix compatibility issue, remove unnecessary command during startup
* Fix NAPT disable issue (esp-lwip NAPT now have a bug, disable then enable NAPT will cause random table index)

## v0.1.5 - 2023-04-17

* Fix compatibility issue, remove unnecessary command during startup

## v0.1.4 - 2023-03-09

* Set default SoftAP DNS during Wi-Fi config
* Add Ping process in example

## v0.1.3 - 2023-03-08

* Fix NAPT enable issue 

## v0.1.2 - 2023-03-06

* Modify the default channel 13 to 6
* Add example link in idf_component.yml

## v0.1.1 - 2023-02-13

* Support IDF5.0
* Change dependency iot_usbh_cdc to private

## v0.1.0 - 2023-02-02

* initial version
