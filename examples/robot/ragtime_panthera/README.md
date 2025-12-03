# Ragtime Panthera Robotic Arm

This project deploys the [Ragtime_Panthera](https://github.com/Ragtime-LAB/Ragtime_Panthera) robotic arm on ESP32-P4, consisting of two components: [follower](./follower/) and [leader](./leader/):

* **Follower**: The main body of the Ragtime_Panthera robotic arm, including DM motor drivers, vision detection, and onboard kinematics. You can implement USB camera-based object recognition and grasping tasks on ESP32-P4.
* **Leader**: A small robotic arm design based on [trlc-dk1](https://github.com/robot-learning-co/trlc-dk1), enabling remote control of the follower robotic arm via ESP-NOW.

<p align="center">
  <img src="https://dl.espressif.com/AE/esp-iot-solution/leader_and_follower.jpg" alt="Ragtime Panthera Follower and Leader" width="90%">
</p>

## Key Features

* **Onboard Object Detection and Kinematics**: The follower robotic arm can independently perform object detection and grasping tasks without requiring additional computing platforms.
* **Remote Control**: The leader robotic arm can synchronize joint data via ESP-NOW to achieve remote control.

<p align="center">
  <img src="https://dl.espressif.com/AE/esp-iot-solution/ragtime_panthera_grasp.gif" alt="Ragtime Panthera Follower Grasp" width="90%">
</p>

<p align="center">
  <img src="https://dl.espressif.com/AE/esp-iot-solution/ragtime_panthera_remote_control.gif" alt="Ragtime Panthera Remote Control" width="90%">
</p>

For more details, please refer to the README files of Follower and Leader.
