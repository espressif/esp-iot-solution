[![Component Registry](https://components.espressif.com/components/espressif/aht20/badge.svg)](https://components.espressif.com/components/espressif/aht20)

# Component: AHT20
I2C driver and definition of AHT20 humidity and temperature sensor. 

Components compatible with AHT30 and AHT21 (AHT21 is deprecated).

See [AHT20 datasheet](http://www.aosong.com/en/products-32.html), [AHT30 datasheet](http://www.aosong.com/en/products-131.html).

## Usage

### I2C Bus Initialization
First, initialize the I2C bus with the following configuration:

```c
#define I2C_MASTER_SCL_IO   CONFIG_I2C_MASTER_SCL   /*!< GPIO number for I2C master clock */
#define I2C_MASTER_SDA_IO   CONFIG_I2C_MASTER_SDA   /*!< GPIO number for I2C master data  */
#define I2C_MASTER_NUM      I2C_NUM_0               /*!< I2C port number for master dev */
#define I2C_MASTER_FREQ_HZ  100000                  /*!< I2C master clock frequency */

const i2c_config_t i2c_bus_conf = {
    .mode = I2C_MODE_MASTER,
    .sda_io_num = I2C_MASTER_SDA_IO,
    .sda_pullup_en = GPIO_PULLUP_ENABLE,
    .scl_io_num = I2C_MASTER_SCL_IO,
    .scl_pullup_en = GPIO_PULLUP_ENABLE,
    .master.clk_speed = I2C_MASTER_FREQ_HZ
};
i2c_bus_handle_t i2c_bus = i2c_bus_create(I2C_MASTER_NUM, &i2c_bus_conf);
```

### AHT20 Sensor Initialization
After I2C bus is ready, initialize the AHT20 sensor:

```c
    aht20_i2c_config_t i2c_conf = {
        .bus_inst = i2c_bus,           // I2C bus instance
        .i2c_addr = AHT20_ADDRRES_0,   // Device address
    };
    aht20_dev_handle_t handle;
    aht20_new_sensor(&i2c_conf, &handle);
```

### Read Temperature and Humidity
The user can periodically call the `aht20_read_temperature_humidity` API to retrieve real-time data:

```c
    uint32_t temperature_raw, humidity_raw;
    float temperature, humidity;

    aht20_read_temperature_humidity(handle, &temperature_raw, &temperature, &humidity_raw, &humidity);
    ESP_LOGI(TAG, "%-20s: %2.2f %%", "humidity is", humidity);
    ESP_LOGI(TAG, "%-20s: %2.2f degC", "temperature is", temperature);
```

### Resource Cleanup
When you no longer need the sensor, delete the resources:

```c
    aht20_del_sensor(handle);
    i2c_bus_delete(&i2c_bus);
```