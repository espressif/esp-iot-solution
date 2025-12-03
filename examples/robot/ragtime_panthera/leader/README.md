# Ragtime Panthera Leader

This is a teleoperation robotic arm designed to control the [Ragtime_Panthera](https://github.com/Ragtime-LAB/Ragtime_Panthera) robotic arm body. Its mechanical structure design is based on the TRLC-DK1 Leader component from the [trlc-dk1](https://github.com/robot-learning-co/trlc-dk1) project, and its structure is largely consistent with that of Ragtime_Panthera.

<p align="center">
  <img src="https://dl.espressif.com/AE/esp-iot-solution/trkc-dk1-leader.png" alt="trkc-dk1-leader" width="80%">
</p>

<p align="center">
  <img src="https://dl.espressif.com/AE/esp-iot-solution/ragtime_panthera_remote_control.gif" alt="Ragtime Panthera Remote Control" width="90%">
</p>

## Key Features

- XL330_M077 bus servo driver: This project provides basic drivers for the XL330_M077 bus servos, including servo scanning, position reading, torque control, reboot, and other features. For communication protocol extensions, you can refer to [DYNAMIXEL Protocol 2.0](https://emanual.robotis.com/docs/en/dxl/protocol2/) for further additions.
- ESP Now: This project uses ESP NOW to synchronize all joint data obtained to the Ragtime Panthera robot arm.

## ESP Now Communication Format

The servo data packet is structured as follows: a header (2 bytes), 7 servo joint data items (7*2 bytes), and a CRC check (2 bytes):

- Header: Fixed value `0xFF 0xFF`
- Joint Data: Each joint angle is represented in radians, multiplied by 100 to convert the floating-point value to an integer, and then split into low and high bytes.
- CRC: Calculated using CRC16 in little-endian order, with an initial value of `UINT16_MAX`.

## Assembly Instructions

When assembling the trlc-dk1-leader, please set each servo to the 180° (2048) position. You can use [Dynamixel Wizard 2](https://emanual.robotis.com/docs/en/software/dynamixel/dynamixel_wizard2/) to assign each servo an ID and set its position in advance. The servo baud rate in this project is 115200, and the IDs start from 1.


> Do not power the XL330_M077 directly via USB while it is under load. The USB power supply may be insufficient when torque mode is enabled; however, you can use USB power to configure basic parameters of the XL330_M077. Additionally, when assembling the servos, if you set the zero position to 0°, it can easily result in 0° becoming 360°. Therefore, it is better to fix the zero position at 180°.

## XL330_M077 Communication Circuit

Since the bus servo uses a single-wire serial interface, it needs to be converted to a standard UART. You can refer to the following circuit design. You can click [here](https://dl.espressif.com/AE/esp-iot-solution/LEADER_ESP32C3_DRIVER_BOARD.zip) to download the PCB Gerber files for PCB prototyping.

![bus_servo_circuit](https://dl.espressif.com/AE/esp-iot-solution/leader_esp32c3_driver_board.jpg)

In addition, you can also refer to the [official reference circuit](https://emanual.robotis.com/docs/en/dxl/x/xl330-m077/#communication-circuit) provided for the XL330-M077.

