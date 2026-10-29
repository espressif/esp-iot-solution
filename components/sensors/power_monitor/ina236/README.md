# Component: INA236

The INA236 device is a 16-bit digital current monitor
with an I2C interface that is
compliant with a wide range of digital bus voltages
such as 1.2 V, 1.8 V, 3.3 V, and 5.0 V. The device
monitors the voltage across an external sense resistor
and reports values for current, bus voltage, and
power.

* This component will show you how to use I2C module read external i2c sensor data, here we use INA236 power monitor.
* Pin assignment:
* master:
  * GPIO21 is assigned as the data signal of i2c master port
  * GPIO13 is assigned as the clock signal of i2c master port
* Connection:
  * connect sda of sensor with GPIO21
  * connect scl of sensor with GPIO13
  * no need to add external pull-up resistors, driver will enable internal pull-up resistors.

## Example of INA236 usage

```c
ina236_handle_t ina236 = NULL;
i2c_bus_handle_t i2c_bus = NULL;
//Step1: Init I2C bus
i2c_config_t conf = {
        .mode = I2C_MODE_MASTER,
        .sda_io_num = I2C_MASTER_SDA_IO,
        .sda_pullup_en = GPIO_PULLUP_ENABLE,
        .scl_io_num = I2C_MASTER_SCL_IO,
        .scl_pullup_en = GPIO_PULLUP_ENABLE,
        .master.clk_speed = I2C_MASTER_FREQ_HZ,
        };
i2c_bus = i2c_bus_create(I2C_MASTER_NUM, &conf);
//Step2: Init ina236
ina236_config_t ina236_cfg = {
        .bus = i2c_bus,
        .dev_addr = INA236_I2C_ADDRESS_DEFAULT,
        .alert_en = false,
        .alert_pin = -1,
        .alert_cb = NULL,
        };
esp_err_t err = ina236_create(&ina236, &ina236_cfg);

//Step3: Get Voltage and Current
float vloatge = 0;
float current = 0;
ina236_get_voltage(ina236, &vloatge);
ina236_get_current(ina236, &current);
```

## Attention

> The current component(power_monitor) will be migrated to the power_measure component after version 0.1.2 and will no longer be maintained thereafter.
