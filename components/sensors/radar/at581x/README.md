[![Component Registry](https://components.espressif.com/components/espressif/at581x/badge.svg)](https://components.espressif.com/components/espressif/at581x)

# Component: AT581X
I2C driver and definition of AT581X radar sensor.

See [AT581X datasheet](https://dl.espressif.com/AE/esp_iot_solution/MS58-3909S68U4-3V3-G-NLS-IIC%20Data%20Sheet%20V1.0.pdf).

## Usage

### Initialization
> Note: You need to initialize the I2C bus first.
```c
    /*
    * Users can customize the `def_conf` configuration
    * or use parameter APIs to meet different wake-up requirements.
    */
    const at581x_default_cfg_t def_cfg = ATH581X_INITIALIZATION_CONFIG();

    const at581x_i2c_config_t i2c_conf = {
        .i2c_port = I2C_MASTER_NUM,
        .i2c_addr = AT581X_ADDRRES_0,

        .int_gpio_num = RADAR_OUTPUT_IO,
        .interrupt_level = 1,
        .interrupt_callback = at581x_isr_callback,

        .def_conf = &def_cfg,
    };
    at581x_new_sensor(&i2c_conf, &handle);
```

### Add interrupt callback
```c
    static void IRAM_ATTR at581x_user_callback(void *arg)
    {
        at581x_i2c_config_t *config = (at581x_i2c_config_t *)arg;
        esp_rom_printf("GPIO intr, val: %d\n", gpio_get_level(config->int_gpio_num));
    }

    at581x_register_interrupt_callback(handle, at581x_user_callback);
```

