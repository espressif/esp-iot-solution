# ChangeLog

## v0.3.0 - 2026-09-14

### Enhancements:

* Replace color detection with esp-dl CNN model (`greencube_detect` component, `greencube_v2` only; old `espdet_pico` model removed)
* Upgrade to ESP-IDF v6.1; dm_motor TWAI driver rewritten around TX-done semaphore gate
* Add torque-triggered gripper hold (NVS-tunable threshold, console `panthera_gripper -t`)
* Raise grasp height to 0.01 m; gripper constants (close position, stall velocity/timeout, hold squeeze) stored in NVS
* Add `reload_vision_matrix` console command for live camera-to-arm calibration reload
* Add AprilTag-based vision calibration scripts (`tools/apriltag_capture.py`, `tools/coordinate_calib.py`)
* Console line-editing, arrow-key history and tab completion enabled over USB-Serial-JTAG
* Add Python kinematics simulation under `sim/`
* Add kinematic gravity compensation (`gravity_comp.cpp`) and unit test app
* Grasp state machine runs successive grasp cycles; state fully resets between runs

### Bug Fixes:

* Fix TWAI TX race: IDF 6.1 clears hw_busy after gate signal, stranding ~1/500 queued frames; added idle poll before send
* Fix false OFFLINE faults: `twai_node_transmit_wait_all_done` with nonzero timeout blocks behind priority-1 timer daemon; replaced with semaphore gate
* Fix console command bytes eaten by linenoise cursor-position query when script or late terminal attach
* Fix grasp task not releasing gripper before homing after IK failure
* Pin UVC/USB host to core 0 to avoid P4 cross-core cache-sync faults; detector verified safe on core 1
* CI build image updated from IDF v5.5 to v6.1

## v0.2.1 - 2026-4-14

### Bug Fix:

* Changed the CAN TX/RX pins to avoid occupying USB SERIAL JTAG
* Replaced the temporary `color_detect` component

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
