# SENSOR HUB

`Sensor hub` is a sensor management component that can realize hardware abstraction, device management and data distribution for sensor devices. From the point of application development, this component has the following features:

1. Support for registering sensor drivers through component approach, enabling users to focus on usage without concerning themselves with the implementation of the driver layer.
2. Provides a unified sensor management and message notification mechanism, allowing users to simply select sensor operation modes, sampling intervals, ranges, etc., and register callback functions for events of interest. This ensures users are notified when the sensor state changes or data collection is complete.

`Sensor hub` can be used as a basic component for sensor applications in various intelligent scenarios such as environment monitoring, motion detection, health management and etc. as it simplifies operation and improves operating efficiency by centralized management of sensors.


## How to register sensor_hub in other sensor drivers

To enable the ``sensor_hub`` to locate the sensor component, please refer to the following procedure:

1. Add a dependency on `sensor_hub` in the `idf_component.yml` file of the sensor driver.
2. Implement specific functions based on the sensor type and register them in the `sensor_hub`.

```c
static imu_impl_t virtual_mpu6050_impl = {
    .init = virtual_imu_init,
    .deinit = virtual_imu_deinit,
    .test = virtual_imu_test,
    .acquire_acce = virtual_imu_acquire_acce,
    .acquire_gyro = virtual_imu_acquire_gyro,
};

SENSOR_HUB_DETECT_FN(IMU_ID, virtual_mpu6050, &virtual_mpu6050_impl);
```

3. Modify the CMakeLists of the sensor driver to link the sensor registration function.

```cmake
target_link_libraries(${COMPONENT_LIB} INTERFACE "-u virtual_imu_init")
```

The `test_apps` of `sensor_hub` provide examples of virtual sensor registration, which you can refer to when registering your own sensor.

## Add component to your project

Please use the component manager command `add-dependency` to add the `sensor_hub` to your project's dependency, during the `CMake` step the component will be downloaded automatically

```
idf.py add-dependency "espressif/sensor_hub=*"
```

Alternatively, you can create `idf_component.yml`. More is in [Espressif's documentation](https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-guides/tools/idf-component-manager.html).

## How to use

[sensor_hub user guide](https://docs.espressif.com/projects/esp-iot-solution/en/latest/sensors/sensor_hub.html)
