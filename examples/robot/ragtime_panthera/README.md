# Ragtime Panthera

A standalone implementation of the [Ragtime Panthera](https://github.com/Ragtime-LAB/Ragtime_Panthera) six-axis robotic arm control system on ESP32-P4. This project enables autonomous target grasping functionality with onboard vision detection, local kinematics computation, and motor control, all running on a single ESP32-P4 microcontroller.

<p align="center">
  <img src="https://dl.espressif.com/AE/esp-iot-solution/ragtime_panthera_grasp.gif" alt="Ragtime Panthera Grasp" width="90%">
</p>

## Key Features

### Onboard Kinematics

The Onboard Kinematics design of Ragtime Panthera is inspired by the [Matrix_and_Robotics_on_STM32](https://github.com/SJTU-RoboMaster-Team/Matrix_and_Robotics_on_STM32), enabling the ESP32-P4 to perform forward and inverse kinematics calculations locally using preset Denavit-Hartenberg (DH) parameters.

### Onboard Vision Detection

Based on [ESP-DL ESPDet-Pico](https://github.com/espressif/esp-dl/tree/master/models/cat_detect) (same preprocess/postprocess as `cat_detect`), the system runs an onboard greencube detector (`greencube_v2_224_esp32p4.espdl`) on JPEG frames from a USB camera. Train custom models with [esp-detection](https://github.com/espressif/esp-detection).

### Autonomous Grasping

Combining vision detection, kinematics computation, and motor control, the system can autonomously detect, locate, and grasp target objects.

### Zero Gravity Mode

After **Enable**, the UI **Zero Gravity** switch puts registered arm joints (J1–J6) into MIT mode and runs ~200 Hz gravity feedforward with light MIT velocity damping, matching the Ragtime Panthera `ZeroGraMode` approach. The gripper stays in position mode; any unregistered joint is treated as fixed at zero. Zero-G is mutually exclusive with Grasp / Go Zero. Support the arm when first enabling; turn Zero-G off before position moves.

### Motor Fault Handling

The DM motor driver reports motor faults and feedback timeouts. On fault or motion timeout the firmware aborts grasp commands and resets the UI switches, but leaves motors enabled so the arm keeps holding torque. Use the Enable switch (or `panthera_enable off`) only when a hard disable is intentionally required. CAN bus-off recovery is automatic. Use `panthera_read_position` or `panthera_motor` to inspect faults and `panthera_clear_error` to clear them.

## Hardware Requirements

- **ESP32-P4-Function-EV-Board**: Main processing unit
- **DM Actuators**: 
  - 4x DM4340
  - 2x DM4310
  - 1x DMH3510
- **USB Camera**: For vision detection (640x480 resolution recommended)
- **CAN Transceiver**: For communication with DM actuators

## Getting Started

### Prerequisites

- [ESP-IDF Release v6.1](https://github.com/espressif/esp-idf/tree/release/v6.1)
- Python 3.10+ and [uv](https://github.com/astral-sh/uv) (for calibration and sim tools)

### Quick Start

1. Clone the repository and navigate to this example:
   ```bash
   cd examples/robot/ragtime_panthera
   ```

2. Configure the project:
   ```bash
   idf.py set-target esp32p4
   idf.py menuconfig
   ```
   See [Project Configuration](#project-configuration) for detailed settings.

3. Build and flash:
   ```bash
   idf.py build flash monitor
   ```

## Robotic Arm Assembly

In the original [Ragtime Panthera](https://github.com/Ragtime-LAB/Ragtime_Panthera) project, a combination of 3 DM4310, 3 DM4340, and 1 DMH3510 actuators is used. However, since the DM4310 at the base has relatively low torque, the robotic arm requires multiple movements to reach target positions when rotating around the Z-axis.

**Recommended Modification**: Replace the base joint motor with a DM4340 for improved performance. This modification provides better torque characteristics without requiring structural changes to the arm assembly.

For detailed assembly instructions, refer to the [Panthera Assembly Video](https://www.bilibili.com/video/BV1AoEAz2Ei8/?spm_id_from=333.337.search-card.all.click).

<p align="center">
  <img src="https://dl.espressif.com/AE/esp-iot-solution/rangtime_panthera_follower.jpg" alt="Ragtime Panthera" width="80%">
</p>

<p align="center">
  <img src="https://dl.espressif.com/AE/esp-iot-solution/robotic_arm_panthera_assembly.png" alt="Ragtime Panthera Assembly" width="80%">
</p>

## Kinematics

The robotic arm kinematics are implemented using DH parameters. You can verify the kinematics calculations using the `roboticstoolbox` library in a Python environment:

### Installation

```bash
pip3 install roboticstoolbox-python numpy
```

### DH Parameters

The DH parameters for the Panthera robotic arm are defined as follows:

```python
from roboticstoolbox import DHRobot, RevoluteDH
import numpy as np

robot = DHRobot(
    [
        RevoluteDH(a=0.0, d=0.1005, alpha=-np.pi / 2, offset=0.0),
        RevoluteDH(a=0.18, d=0.0, alpha=0.0, offset=np.deg2rad(180)),
        RevoluteDH(a=0.188809, d=0.0, alpha=0.0, offset=np.deg2rad(162.429)),
        RevoluteDH(a=0.08, d=0.0, alpha=-np.pi / 2, offset=np.deg2rad(17.5715)),
        RevoluteDH(a=0.0, d=0.0, alpha=np.pi / 2, offset=np.deg2rad(90)),
        RevoluteDH(a=0.0, d=0.184, alpha=np.pi / 2, offset=np.deg2rad(-90)),
    ],
    name="Panther6DOF"
)
```

![panthera_assembly](https://dl.espressif.com/AE/esp-iot-solution/robotic_arm_panthera_roboticstoolbox_python.png)

## Vision Calibration

The camera is fixed separately from the arm (eye-to-hand). A 3x3 affine matrix maps pixel coordinates to arm-frame positions. Re-calibrate whenever the camera or arm base moves. Until calibrated the matrix is zeros and every grasp aims at the origin.

### Setup

Requires [uv](https://github.com/astral-sh/uv). No setup step; deps are declared inline in each script.

### Steps

1. **Measure tag positions.** Place an AprilTag 36h11 board (ids 0-8) on the table. Measure each tag in the arm's frame (`panthera_fk` prints the same frame) and fill in the `WORLD` table in `tools/coordinate_calib.py`. Keep all tags at the same height.

2. **Capture pixel positions.**
   ```bash
   uv run tools/apriltag_capture.py --size 640x480 --calib-out calib_pixels.json
   ```
   Must be 640x480 (what the firmware streams). The camera modes are different fields of view, not crops.

3. **Fit the matrix.**
   ```bash
   uv run tools/coordinate_calib.py --pixels calib_pixels.json
   ```
   Check the per-tag errors. Aim for RMS under 10 mm. Large errors are usually wrong hand measurements.

4. **Store on device.** Paste the printed command into the console. It takes effect immediately (no reboot needed):
   ```
   panthera_set_vision_matrix -1 <m1> -2 <m2> ... -9 <m9>
   ```

5. **Verify.**
   ```bash
   uv run tools/coordinate_calib.py --map 248.7,226.4
   ```
   Or on the device: `panthera_goto_pixel -u 248.7 -v 226.4`

### Troubleshooting

| Symptom | Fix |
|---------|-----|
| `numpy is missing` | Wrong python. Use the one with the deps installed. |
| `cannot open /dev/video0` | Camera off the bus or held by another program. |
| Few tags found | Move closer, add light, keep `--decimate` at 1.0. |
| `WARNING: sampled at 1280x720` | Re-capture at `--size 640x480`. |
| One tag has huge error | Re-measure that tag's world position. |

## Console Control

When `CONSOLE_CONTROL` is enabled in the project configuration, you can control the robotic arm directly from the terminal. The following commands are available:

Every UI control has a console command, and both run the same code. `panthera_enable`, `panthera_zero_g`, `panthera_grasp` and `panthera_goto_zero` each drive the switch or button on screen, apply the same guards, and leave the firmware in the same state as a touch would. The console and the screen cannot disagree.

### Basic Control Commands

| Command | Description | Usage |
|---------|-------------|-------|
| `panthera_enable` | Enable or disable all motors, exactly as the UI **Enable** switch does | `panthera_enable <on\|off>` |
| `panthera_goto_zero` | Move all joints to zero position, as the UI **Go Zero** button does | `panthera_goto_zero` |
| `panthera_zero_g` | Enter or leave Zero Gravity, as the UI **Zero Gravity** switch does | `panthera_zero_g <on\|off>` |
| `panthera_grasp` | Grasp the target the camera currently sees, as the UI **Grasp** button does | `panthera_grasp` |
| `panthera_set_zero` | Set current position as zero for all motors (**console only**, not on UI — dangerous) | `panthera_set_zero` |
| `panthera_goto_position` | Move end-effector to specified Cartesian coordinates | `panthera_goto_position -x <x> -y <y> -z <z>` |
| `panthera_goto_pixel` | Map an image pixel to a world position with the stored vision matrix and move there | `panthera_goto_pixel -u <u> -v <v> [-z <m>] [-n]` |
| `panthera_read_position` | Read position, fault and online state of all joints | `panthera_read_position` |
| `panthera_fk` | Forward kinematics from the current J1-J6 angles; disabled or missing joints count as 0 | `panthera_fk` |
| `panthera_set_vision_matrix` | Store the 3x3 pixel-to-world matrix produced by calibration | `panthera_set_vision_matrix -1 <m1> ... -9 <m9>` |
| `panthera_get_vision_matrix` | Print the stored vision matrix | `panthera_get_vision_matrix` |
| `panthera_clear_error` | Clear error state of all motors (re-enable afterwards) | `panthera_clear_error` |
| `panthera_gripper` | Show or set the gripper close position and contact torque grasp uses (saved to NVS) | `panthera_gripper [-c <rad>] [-t <Nm>]` |
| `panthera_motor` | Control or inspect one registered motor | `panthera_motor -m <master_id> -a <enable\|disable\|clear\|zero\|status\|pos> [-p <rad>] [-v <rad/s>]` |

### Example Usage

```bash
# Enable motors
panthera_enable on

# Return every joint to its zero position
panthera_goto_zero

# Hand-guide the arm (support it first), then leave zero gravity
panthera_zero_g on
panthera_zero_g off

# Grasp whatever the camera currently sees
panthera_grasp

# Move to a specific position (in meters)
panthera_goto_position -x 0.3 -y 0.2 -z 0.1

# Move to where an image pixel maps to, using the stored vision matrix
panthera_goto_pixel -u 248.7 -v 226.4        # move there
panthera_goto_pixel -u 248.7 -v 226.4 -n     # print the solution, do not move

# Read the joints, then check where the arm thinks its tip is
panthera_read_position
panthera_fk

# Tune how hard grasp closes the gripper (larger = tighter), then grasp again
panthera_gripper -c 1.5
panthera_gripper -t 0.4   # hold as soon as the jaws feel 0.4 Nm

# Test a registered motor
panthera_motor -m 0x11 -a enable
panthera_motor -m 0x11 -a pos -p 0.5
panthera_motor -m 0x11 -a status
panthera_motor -m 0x11 -a disable

# Set calibration matrix
panthera_set_vision_matrix -1 -0.000002 -2 -0.001861 -3 0.867622 -4 -0.001776 -5 -0.000079 -6 0.450608 -7 0.000000 -8 -0.000000 -9 0.039700

# Verify the matrix
panthera_get_vision_matrix
```

## Project Configuration

Navigate to `(Top) → Ragtime Panthera Configuration` in `idf.py menuconfig` to configure the following options:

### Console Control

- **`CONSOLE_CONTROL`**: Enable direct control of the robotic arm via terminal. When enabled, you can perform basic operations including kinematics testing, joint movement, and motor enabling/disabling.

### Hardware Configuration

Configure the communication interfaces:

- **`CAN_TX_NUM`**: CAN transceiver TX pin (default: GPIO 21)
- **`CAN_RX_NUM`**: CAN transceiver RX pin (default: GPIO 22)

**Note**: Ensure these pins match your hardware connections for proper motor communication.

## Building and Flashing

### Build, Flash and Monitor

```bash
idf.py build flash monitor
```

### Clean Build

```bash
idf.py fullclean
```

## Troubleshooting

### Motors Not Responding

- Verify CAN transceiver connections (TX/RX pins)
- Check motor power supply
- Verify the motor's MasterID is configured (e.g. 0x11) in the DM host tool — motors with MasterID 0x00 never send feedback
- Use `panthera_motor -m <master> -a status` to check one motor's feedback age and fault state
- A repeated "TWAI bus off" log means no node is ACKing frames (wiring/bitrate mismatch); recovery is automatic once the bus is healthy
- Ensure motors are properly initialized via `panthera_enable on`

### Vision Detection Not Working

- Verify USB camera connection and power
- Check camera resolution matches expected 640x480
- Ensure calibration matrix is properly set
### Kinematics Errors

- Verify DH parameters match your hardware configuration
- Check joint limits and workspace boundaries
- Ensure inverse kinematics solution exists for target position

## References

- [Ragtime Panthera](https://github.com/Ragtime-LAB/Ragtime_Panthera) - Original robotic arm project
- [Matrix_and_Robotics_on_STM32](https://github.com/SJTU-RoboMaster-Team/Matrix_and_Robotics_on_STM32) - Kinematics implementation reference
- [ESP-DL Cat Detect / ESPDet-Pico](https://github.com/espressif/esp-dl/tree/master/models/cat_detect) - Detection pipeline mirrored by `greencube_detect`
- [ESP-Detection](https://github.com/espressif/esp-detection) - Custom object detection training
