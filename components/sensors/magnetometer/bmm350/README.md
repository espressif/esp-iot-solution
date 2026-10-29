# BMM350 SensorAPI

> **Bosch Sensortec's [BMM350](https://www.bosch-sensortec.com/media/boschsensortec/downloads/datasheets/bst-bmm350-ds001.pdf) SensorAPI**
>
> This driver component is based on the Bosch Sensortec's [BMM350_SensorAPI](https://github.com/boschsensortec/BMM350_SensorAPI) v1.4.0,
> with modifications to `common/*` to adapt to the ESP platform.

## Sensor Overview

The BMM350 is a 3-axis magnetic sensor which operates in automatic mode or triggered mode.
The magnetic-to-digital conversion technology is based on TMR (tunnel magneto resistance).
The BMM350 has an excellent temperature behaviour with an outstanding low temperature coefficient of the offset (TCO) and temperature coefficient of the sensitivity (TCS).

### Applications

1. Virtual, augmented and mixed reality applications
2. High-end gaming applications
3. Platform stabilization applications such as image stabilization, or indoor navigation and dead-reckoning, for example in robotics applications.
4. Magnetic heading information
5. Tilt-compensated electronic compass for map rotation, navigation and augmented reality
6. Gyroscope calibration in 9-DoF applications for mobile devices
7. In-door navigation, e.g. step counting in combination with accelerometer
8. Gaming (AR/VR)

## Direct I2C

Bind the ESP platform callbacks, then use the Bosch API. Address is `0x14` or `0x15` (ADSEL). Data is `float` in micro-tesla. Glue is a file-level singleton: one BMM350 per process. Do not call the bus setter, `bmm350_set_i2c_address()`, or `bmm350_interface_init()` while another task is using the sensor.

```c
struct bmm350_dev dev;
struct bmm350_mag_temp_data data;

bmm350_set_i2c_bus_handle(i2c_bus);
bmm350_set_i2c_address(BMM350_I2C_ADSEL_SET_LOW);
bmm350_interface_init(&dev);
bmm350_init(&dev);

bmm350_set_odr_performance(BMM350_DATA_RATE_100HZ, BMM350_AVERAGING_4, &dev);
bmm350_enable_axes(BMM350_X_EN, BMM350_Y_EN, BMM350_Z_EN, &dev);
bmm350_set_powermode(BMM350_NORMAL_MODE, &dev);

bmm350_get_compensated_mag_xyz_temp_data(&data, &dev);
```

For a caller-owned `i2c_master_bus_handle_t` (Board Manager, `i2c_new_master_bus()`), use `bmm350_set_i2c_master_bus_handle()` instead. Device clock is 100 kHz, timeout 200 ms. `CONFIG_I2C_BUS_BACKWARD_CONFIG` must be off. On a failed device removal, transfers are blocked until a later setter, init, or deinit succeeds. The caller still owns the bus.

See [bmm350.h](./bmm350.h) and [common/bmm350_common.h](./common/bmm350_common.h).
