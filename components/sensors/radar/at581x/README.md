[![Component Registry](https://components.espressif.com/components/espressif/at581x/badge.svg)](https://components.espressif.com/components/espressif/at581x)

# Component: AT581X
I2C driver and definition of AT581X radar sensor.

See [AT581X datasheet](https://dl.espressif.com/AE/esp_iot_solution/MS58-3909S68U4-3V3-G-NLS-IIC%20Data%20Sheet%20V1.0.pdf).

## Usage

### I2C Bus Initialization
First, initialize the I2C bus with the following configuration:

```c
#define I2C_MASTER_SCL_IO   CONFIG_I2C_MASTER_SCL    /*!< GPIO number used for I2C master clock */
#define I2C_MASTER_SDA_IO   CONFIG_I2C_MASTER_SDA    /*!< GPIO number used for I2C master data  */
#define I2C_MASTER_NUM      I2C_NUM_0                /*!< I2C port number for master dev */
#define I2C_MASTER_FREQ_HZ  100000                   /*!< I2C master clock frequency */
#define RADAR_OUTPUT_IO     CONFIG_RADAR_OUTPUT      /*!< Radar output IO */

const i2c_config_t i2c_bus_conf = {
    .mode = I2C_MODE_MASTER,
    .sda_io_num = (gpio_num_t)I2C_MASTER_SDA_IO,
    .sda_pullup_en = GPIO_PULLUP_DISABLE,
    .scl_io_num = (gpio_num_t)I2C_MASTER_SCL_IO,
    .scl_pullup_en = GPIO_PULLUP_DISABLE,
    .master.clk_speed = I2C_MASTER_FREQ_HZ
};
i2c_bus_handle_t i2c_bus = i2c_bus_create(I2C_MASTER_NUM, &i2c_bus_conf);
```

### AT581X Sensor Initialization
After I2C bus is ready, initialize the AT581X sensor:

```c
    /*
    * Users can customize the `def_conf` configuration
    * or use parameter APIs to meet different wake-up requirements.
    */
    const at581x_default_cfg_t def_cfg = ATH581X_INITIALIZATION_CONFIG();

    at581x_i2c_config_t i2c_conf = {
        .bus_inst = i2c_bus,           // I2C bus instance
        .i2c_addr = AT581X_ADDRRES_0,  // Device address

        .int_gpio_num = RADAR_OUTPUT_IO,
        .interrupt_level = 1,
        .interrupt_callback = at581x_isr_callback,

        .def_conf = &def_cfg,
    };
    at581x_dev_handle_t handle;
    at581x_new_sensor(&i2c_conf, &handle);
```

### Add Interrupt Callback
```c
    static void IRAM_ATTR at581x_isr_callback(void *arg)
    {
        at581x_i2c_config_t *config = (at581x_i2c_config_t *)arg;
        esp_rom_printf("GPIO[%d] intr, val: %d\n", config->int_gpio_num, gpio_get_level(config->int_gpio_num));
    }

    at581x_register_interrupt_callback(handle, at581x_isr_callback);
```

### Resource Cleanup
When you no longer need the sensor, delete the resources:

```c
    at581x_del_sensor(handle);
    i2c_bus_delete(&i2c_bus);
```

