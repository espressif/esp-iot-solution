# ChangeLog

## v0.2.1 - 2026-01-12

### Bug Fixes:

* Fixed incorrect interface number in ECM_SET_ETHERNET_PACKET_FILTER request. Changed from hardcoded 0 to use actual interface number (`ecm->itf_num`).

## v0.2.0 - 2025-10-15

### Bug Fixes:

* Added configuration `ECM_AUTO_LINK_UP_AFTER_NO_NOTIFICATION` to improved driver compatibility with 4G and Ethernet modules.
* Fixed hot-plugging stability issues.
* Added test cases to verify the functionality of the ECM driver.

## v0.1.0 - 2025-04-22

* publish official version
