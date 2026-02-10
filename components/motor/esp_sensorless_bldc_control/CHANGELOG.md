# ChangeLog

## v1.0.2 - 2025-10-24

### Bug fix:

* Change the default clock source for the LEDC timer to utilise the LEDC_USE_XTAL_CLK source when the target platform is H2、C6、P4.

## v1.0.1 - 2025-10-22

### Enhancements

* Add fan support within matter-one project
* Place alignment_comparer_get_value() and read_comparer_on_full() functions in IRAM.
* Add 'is_running' flag to assist upper layer in synchronising the status of the BLDC controller.
* Modify the registration method of mcpwm_timer_event_callbacks_t to utilise dynamic memory allocation.

## v1.0.0 - 2024-9-20

* Component version maintenance.

## v0.3.0 - 2024-6-20

### Bug fix

* Fix drag time and modify the filter cap to type uint32_t
* Fix Comparator Acquisition Method for MCPWM Trigger

## v0.2.0 - 2023-11-14

### Enhancements

* Add test apps: step by step

### Bug fix

* Fix error speed calculation.

## v0.1.0 - 2023-11-14

### Enhancements

* Init version
