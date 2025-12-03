# Ragtime Panthera Follower

A standalone implementation of the [Ragtime Panthera](https://github.com/Ragtime-LAB/Ragtime_Panthera) six-axis robotic arm control system on ESP32-P4. This project enables autonomous target grasping functionality with onboard vision detection, local kinematics computation, and motor control, all running on a single ESP32-P4 microcontroller.

<p align="center">
  <img src="https://dl.espressif.com/AE/esp-iot-solution/ragtime_panthera_grasp.gif" alt="Ragtime Panthera Follower Grasp" width="90%">
</p>

## Key Features

### Onboard Kinematics

The Onboard Kinematics design of Ragtime Panthera is inspired by the [Matrix_and_Robotics_on_STM32](https://github.com/SJTU-RoboMaster-Team/Matrix_and_Robotics_on_STM32), enabling the ESP32-P4 to perform forward and inverse kinematics calculations locally using preset Denavit-Hartenberg (DH) parameters.

### Onboard Vision Detection

Based on [color_detect](https://github.com/espressif/esp-dl/tree/master/models/color_detect), the system can recognize target objects by color in real-time. The vision system processes JPEG frames from a USB camera and performs color detection. You can also use [esp-detection](https://github.com/espressif/esp-detection) to train custom object detection models.

### Teleoperation

The project operates as a follower device, synchronizing joint angle information with a leader arm via ESP-NOW. This allows intuitive control of the Panthera robotic arm using a leader device, enabling applications such as remote operation and teaching-by-demonstration.

### Autonomous Grasping

Combining vision detection, kinematics computation, and motor control, the system can autonomously detect, locate, and grasp target objects.

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

- [ESP-IDF Release V5.5](https://github.com/espressif/esp-idf/tree/release/v5.5)
- Python 3.8+ (for calibration tools)

### Quick Start

1. Clone the repository and navigate to this example:
   ```bash
   cd examples/robot/ragtime_panthera/follower
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
  <img src="https://dl.espressif.com/AE/esp-iot-solution/rangtime_panthera_follower.jpg" alt="Ragtime Panthera Follower" width="80%">
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

This project uses an **eye-to-hand** configuration, meaning the camera is fixed at a specific position separate from the robotic arm. To enable the robotic arm to grasp objects detected in the camera view, you need to calibrate the transformation matrix that converts pixel coordinates to real-world spatial coordinates.

### Calibration Process

1. **Collect Calibration Points**: Move the robotic arm to known spatial positions and record:
   - **B**: The end-effector position in real-world coordinates (meters)
   - **A**: The corresponding pixel coordinates in the camera image

2. **Generate Transformation Matrix**: Use the provided calibration script:
   ```bash
   python3 tools/generate_vision_matrix.py
   ```

3. **Set the Matrix**: Use the console command to store the calibration matrix:
   ```bash
   panthera_set_version_matrix -1 <m1> -2 <m2> -3 <m3> -4 <m4> -5 <m5> -6 <m6> -7 <m7> -8 <m8> -9 <m9>
   ```

### Calibration Tips

- Collect at least 4-6 calibration points for better accuracy
- Ensure calibration points are well-distributed across the workspace
- Re-calibrate if the camera position changes

## Console Control

When `CONSOLE_CONTROL` is enabled in the project configuration, you can control the robotic arm directly from the terminal. The following commands are available:

### Basic Control Commands

| Command | Description | Usage |
|---------|-------------|-------|
| `panthera_enable` | Enable or disable all motors | `panthera_enable <on\|off>` |
| `panthera_goto_zero` | Move all joints to zero position | `panthera_goto_zero` |
| `panthera_set_zero` | Set current position as zero for all motors | `panthera_set_zero` |
| `panthera_goto_position` | Move end-effector to specified Cartesian coordinates | `panthera_goto_position -x <x> -y <y> -z <z>` |
| `panthera_read_position` | Read all joints position info | `panthera_read_position` |

### Vision Matrix Commands

| Command | Description | Usage |
|---------|-------------|-------|
| `panthera_set_version_matrix` | Set the vision calibration matrix | `panthera_set_version_matrix -1 <m1> -2 <m2> ... -9 <m9>` |
| `panthera_get_version_matrix` | Read and display the current calibration matrix | `panthera_get_version_matrix` |

### Example Usage

```bash
# Enable motors
panthera_enable on

# Move to a specific position (in meters)
panthera_goto_position -x 0.3 -y 0.2 -z 0.1

# Set calibration matrix
panthera_set_version_matrix -1 -0.000002 -2 -0.001861 -3 0.867622 -4 -0.001776 -5 -0.000079 -6 0.450608 -7 0.000000 -8 -0.000000 -9 0.039700

# Verify the matrix
panthera_get_version_matrix
```

## Project Configuration

Navigate to `(Top) → Panthera Follower Configuration` in `idf.py menuconfig` to configure the following options:

### Console Control

- **`CONSOLE_CONTROL`**: Enable direct control of the robotic arm via terminal. When enabled, you can perform basic operations including kinematics testing, joint movement, and motor enabling/disabling.

### Hardware Configuration

Configure the communication interfaces:

- **`CAN_TX_NUM`**: CAN transceiver TX pin (default: GPIO 24)
- **`CAN_RX_NUM`**: CAN transceiver RX pin (default: GPIO 25)

**Note**: Ensure these pins match your hardware connections for proper motor communication.

### Leader and Follower Gripper Mapping

Map the gripper angle from the Leader robotic arm to the Panthera robotic arm:

- **`LEADER_INPUT_MIN`**: Minimum input angle (in radians × 100, default: -58 for -0.58 rad)
- **`LEADER_INPUT_MAX`**: Maximum input angle (in radians × 100, default: 0 for 0.0 rad)
- **`FOLLOWER_OUTPUT_MIN`**: Minimum output angle (in radians × 100, default: 0 for 0.0 rad)
- **`FOLLOWER_OUTPUT_MAX`**: Maximum output angle (in radians × 100, default: 628 for 6.28 rad)

By default, the Leader robotic arm starts at the maximum gripper position.

### Leader and Follower Angle Inversion

Configure whether the joint angles of the leader robotic arm and the follower robotic arm need to be inverted.

Due to the mechanical installation, the rotation directions of the leader and follower arms may differ. You can use the right-hand rule to determine the positive and negative rotation directions of each joint. If the positive rotation direction of a corresponding joint is different, you need to set INVERT to 1 for that joint.

In this project, the leader and follower have different positive rotation directions for Joint 3, so `SERVO3_JOINT3_INVERT` is set to 1.

### Receiver Serial Flash Config

Since the current [ESP-HOSTED-MCU](https://github.com/espressif/esp-hosted-mcu) does not support [ESP-NOW](https://docs.espressif.com/projects/esp-idf/en/stable/esp32/api-reference/network/esp_now.html), we independently implemented ESP-NOW packet reception on the ESP32-C6 side and used [esp-serial-flasher](https://github.com/espressif/esp-serial-flasher) to download the firmware to the ESP32-C6.

- **`ENABLE_UPDATE_C6_FLASH`**: Controls whether ESP32-P4 flashes firmware to ESP32-C6. If the ESP32-C6 firmware is stable and fully debugged, you can disable this option to speed up UI loading.
- **`SERIAL_FLASH_RX_NUM`**: GPIO pin connected to ESP32-C6 TX0 on ESP32-P4-Function-EV-Board (default: GPIO 6)
- **`SERIAL_FLASH_TX_NUM`**: GPIO pin connected to ESP32-C6 RX0 on ESP32-P4-Function-EV-Board (default: GPIO 5)
- **`SERIAL_FLASH_RESET_NUM`**: GPIO pin connected to ESP32-C6 EN (reset) on ESP32-P4-Function-EV-Board (default: GPIO 54)
- **`SERIAL_FLASH_BOOT_NUM`**: GPIO pin connected to ESP32-C6 BOOT on ESP32-P4-Function-EV-Board (default: GPIO 53)

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
- Ensure motors are properly initialized via `panthera_enable on`

### Vision Detection Not Working

- Verify USB camera connection and power
- Check camera resolution matches expected 640x480
- Ensure calibration matrix is properly set
- Adjust HSV color detection parameters if needed

### Kinematics Errors

- Verify DH parameters match your hardware configuration
- Check joint limits and workspace boundaries
- Ensure inverse kinematics solution exists for target position

## References

- [Ragtime Panthera](https://github.com/Ragtime-LAB/Ragtime_Panthera) - Original robotic arm project
- [Matrix_and_Robotics_on_STM32](https://github.com/SJTU-RoboMaster-Team/Matrix_and_Robotics_on_STM32) - Kinematics implementation reference
- [ESP-DL Color Detection](https://github.com/espressif/esp-dl/tree/master/models/color_detect) - Vision detection model
- [ESP-Detection](https://github.com/espressif/esp-detection) - Custom object detection training
