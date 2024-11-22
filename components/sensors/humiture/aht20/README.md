[![Component Registry](https://components.espressif.com/components/espressif/aht20/badge.svg)](https://components.espressif.com/components/espressif/aht20)

# Component: AHT20
I2C driver and definition of AHT20 humidity and temperature sensor. 

Components compatible with AHT30 and AHT21 (AHT21 is deprecated).

See [AHT20 datasheet](http://www.aosong.com/en/products-32.html), [AHT30 datasheet](http://www.aosong.com/en/products-131.html).


## Usage

### Initialization
> Note: Note: You need to initialize the I2C bus first.
```c
    aht20_i2c_config_t i2c_conf = {
        .i2c_port = I2C_MASTER_NUM,
        .i2c_addr = AHT20_ADDRRES_0,
    };
    aht20_new_sensor(&i2c_conf, &handle);
```

### Read data
> The user can periodically call the aht20_read_temp_hum API to retrieve real-time data.
```c
    uint32_t temp_raw, hum_raw;
    float temp, hum;

    aht20_read_temp_hum(aht20, &temp_raw, &temp, &hum_raw, &hum);
    ESP_LOGI(TAG, "Humidity      : %2.2f %%", hum);
    ESP_LOGI(TAG, "Temperature   : %2.2f degC", temp);
```