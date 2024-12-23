# Component: BME280

The BME280 is as combined digital humidity, pressure and temperature sensor based on proven sensing principles. The sensor module is housed in an extremely compact metal-lid LGA package with a footprint of only 2.5 × 2.5 mm² with a height of 0.93 mm. Its small dimensions and its low power consumption allow the implementation in battery driven devices such as handsets, GPS modules or watches.


## Add component to your project

Please use the component manager command `add-dependency` to add the `bme280` to your project's dependency, during the `CMake` step the component will be downloaded automatically

```
idf.py add-dependency "espressif/bme280=*"
```

## Example of BME280 usage


Pin assignment:

* master:
  * GPIO2 is assigned as the clock signal of i2c master port
  * GPIO1 is assigned as the data signal of i2c master port
* Connection:
  * connect sda of sensor with GPIO1
  * connect scl of sensor with GPIO2


```c
static i2c_bus_handle_t i2c_bus = NULL;
static bme280_handle_t bme280 = NULL;

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

//Step2: Init bme280
bme280 = bme280_create(i2c_bus, BME280_I2C_ADDRESS_DEFAULT);
bme280_default_init(bme280);

//Step3: Read temperature, humidity and pressure
float temperature = 0.0, humidity = 0.0, pressure = 0.0;
bme280_read_temperature(bme280, &temperature);
bme280_read_humidity(bme280, &humidity);
bme280_read_pressure(bme280, &pressure);
```