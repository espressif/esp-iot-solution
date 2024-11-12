# Component: HDC2010

The HDC2010 is an integrated humidity and temperature sensor that provides high accuracy measurements with very low power consumption, in an ultra-compact WLCSP (Wafer Level Chip Scale Package). Additionally, HDC2010 provides high accuracy measurement capability for a wide range of environmental monitoring applications and Internet of Things (IoT) such as smart thermostats, smart home assistants and wearables.

## Add component to your project

Please use the component manager command `add-dependency` to add the `hdc2010` to your project's dependency, during the `CMake` step the component will be downloaded automatically

```
idf.py add-dependency "espressif/hdc2010=*"
```

## Example of HDC2010 usage

Pin assignment:

* master:
  * GPIO2 is assigned as the clock signal of i2c master port
  * GPIO1 is assigned as the data signal of i2c master port
* Connection:
  * connect sda of sensor with GPIO1
  * connect scl of sensor with GPIO2

```c
static i2c_bus_handle_t i2c_bus = NULL;
static hdc2010_handle_t hdc2010 = NULL;

//Step1: Init I2C bus
i2c_config_t conf = {
    .mode = I2C_MODE_MASTER,
    .sda_io_num = HDC2010_I2C_MASTER_SDA_IO,
    .sda_pullup_en = GPIO_PULLUP_ENABLE,
    .scl_io_num = HDC2010_I2C_MASTER_SCL_IO,
    .scl_pullup_en = GPIO_PULLUP_ENABLE,
    .master.clk_speed = HDC2010_I2C_MASTER_FREQ_HZ,
};
i2c_bus = i2c_bus_create(HDC2010_I2C_MASTER_NUM, &conf);

//Step2: Init hdc2010
hdc2010 = hdc2010_create(i2c_bus, HDC2010_ADDR_PIN_SELECT_GND);

//Step3: Read temperature and humidity
float temperature = hdc2010_get_temperature(hdc2010);
float humidity = hdc2010_get_humidity(hdc2010);
```
