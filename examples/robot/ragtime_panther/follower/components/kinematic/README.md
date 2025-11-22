# 6DOF Kinematic

The local kinematic design of Ragtime Panthera is based on [Matrix_and_Robotics_on_STM32](https://github.com/SJTU-RoboMaster-Team/Matrix_and_Robotics_on_STM32).

In addition, you can use the Python library `roboticstoolbox` for online simulation and debugging of the robotic arm. The DH parameters of the robotic arm are as follows:

```python
robot = DHRobot(
    [
        RevoluteDH(a=0.0, d=0.1005, alpha=-np.pi / 2, offset=0.0),
        RevoluteDH(a=0.18, d=0.0, alpha=0.0, offset=np.deg2rad(180)),
        RevoluteDH(a=0.188809, d=0.0, alpha=0.0, offset=np.deg2rad(162.429)),
        RevoluteDH(a=0.08, d=0.0, alpha=-np.pi / 2, offset=np.deg2rad(17.5715)),
        RevoluteDH(a=0.0, d=0.0, alpha=np.pi / 2, offset=np.deg2rad(90)),
        RevoluteDH(a=0.0, d=0.184, alpha=np.pi / 2, offset=np.deg2rad(-90)),
    ],
    name="Ragtime_Panthera"
)
```
