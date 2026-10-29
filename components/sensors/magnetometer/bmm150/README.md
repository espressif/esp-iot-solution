# BMM150 SensorAPI

> **Bosch Sensortec's [BMM150](https://www.bosch-sensortec.com/media/boschsensortec/downloads/datasheets/bst-bmm150-ds001.pdf) SensorAPI**
>
> This driver component is based on the Bosch Sensortec's [BMM150_SensorAPI](https://github.com/boschsensortec/BMM150_SensorAPI) v2.0.0,
> with modifications to `common/*` to adapt to the ESP platform.

## Sensor Overview

BMM150 is a standalone geomagnetic sensor for consumer market applications. Performance and features are carefully tuned and match the requirements of 3-axis mobile applications such as electronic compass, navigation or augmented reality.

### Features

- Magnetic heading information
- Tilt-compensated electronic compass
- Gyroscope calibration in 9-DoF applications for mobile devices
- Indoor navigation
- Gaming

## Interfaces

The sensor can be reached over two paths, which decide the API to use:

| Path       | Wiring                                 | Extra dependency                    | Entry point                                          |
| ---------- | -------------------------------------- | ----------------------------------- | ---------------------------------------------------- |
| Direct I2C | BMM150 on the host I2C bus, `i2c_bus` or native `i2c_master` | none              | [common/bmm150_common.h](./common/bmm150_common.h)   |
| BMI270 AUX | BMM150 on the BMI270 auxiliary I2C bus | `espressif/bmi270_sensor` >= 0.2.1  | [bmm150_aux_adapter.h](./bmm150_aux_adapter.h)       |

Both paths share the Bosch API in [bmm150.h](./bmm150.h) for configuration and data readout. Magnetometer data is reported as `float` in micro-tesla, because the component builds the SensorAPI with `BMM150_USE_FLOATING_POINT`.

### Direct I2C

Bind the ESP platform callbacks, then use the Bosch API. Address is `0x10`, `0x11`, `0x12` or `0x13`, depending on CSB and SDO. The I2C state lives in `dev->intf_ptr`, so several sensors can share one bus:

```c
struct bmm150_dev dev;
struct bmm150_settings settings = {0};
struct bmm150_mag_data data;

bmm150_interface_init_from_master_bus(&dev, bus, BMM150_DEFAULT_I2C_ADDRESS, 400000);
bmm150_init(&dev);

settings.preset_mode = BMM150_PRESETMODE_REGULAR;
bmm150_set_presetmode(&settings, &dev);
settings.pwr_mode = BMM150_POWERMODE_NORMAL;
bmm150_set_op_mode(&settings, &dev);

bmm150_read_mag_data(&data, &dev);

bmm150_interface_deinit_device(&dev);
```

The caller selects the device clock and keeps the bus. Multiple sensors can share one bus, and each device must be released with `bmm150_interface_deinit_device()`. Native I2C requires `CONFIG_I2C_BUS_BACKWARD_CONFIG` to be disabled.

The existing file-level APIs remain available for single-sensor applications.

### BMI270 AUX

Add `espressif/bmi270_sensor` (>= 0.2.1) to the project manifest. The adapter is compiled only when that component is part of the build, so a direct I2C project never pulls it in.

Configure the BMI270 auxiliary interface in manual mode first, then bring up the adapter:

```c
bmi270_sensor_create(i2c_bus, &bmi_handle, bmi270_config_file, 0);

bmi270_aux_config_t aux_cfg = {
    .aux_en = BMI2_ENABLE,
    .manual_en = BMI2_ENABLE,
    .man_rd_burst = BMI2_AUX_READ_LEN_1,
    .aux_rd_burst = BMI2_AUX_READ_LEN_1,
    .odr = BMI2_AUX_ODR_100HZ,
    .i2c_device_addr = BMM150_DEFAULT_I2C_ADDRESS,
    .read_addr = BMM150_REG_CHIP_ID,
};
bmi270_aux_set_config(bmi_handle, &aux_cfg);

bmm150_aux_config_t cfg = {
    .bmi270_dev = bmi_handle,
};
bmm150_aux_adapter_init(&cfg, &bmm150_handle);
```

Enable `BMI2_AUX` together with the other sensors through `bmi270_sensor_enable()`, then read with `bmm150_aux_adapter_read_mag_data()`.

---
