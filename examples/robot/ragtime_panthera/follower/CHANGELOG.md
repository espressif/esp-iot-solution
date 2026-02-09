# ChangeLog

## v0.2.0 - 2026-2-1

### Enhancements:

* Support switching color detection HSV range via console command

### Bug Fix:

* Fix `grasp_task` hanging when executing grasp task with motors not powered on
* Fix issue where `grasp_task` state is not reset to IDLE when motors are disabled
* Prevent SYNC switch from being toggled when motors are not enabled

## v0.1.0 - 2025-12-3

### Enhancements:

* Support position, velocity, and MIT mode control for DM actuators
* Support Onboard Kinematics
* Support Onboard Vision Detection, currently using color detection
* Support ESP-NOW for receiving joint angles from Leader robotic arm
