# Component: VEML6075

The VEML6075 senses UVA and UVB light and incorporates photodiode, amplifiers, and analog / digital circuits into a single chip using a CMOS process. When the UV sensor is applied, it is able to detect UVA and UVB intensity to provide a measure of the signal strength as well as allowing for UVI measurement.

## Add component to your project

Please use the component manager command `add-dependency` to add the `veml6075` to your project's dependency, during the `CMake` step the component will be downloaded automatically

```
idf.py add-dependency "espressif/veml6075=*"
```

## Example of VEML6075 usage

* This component will show you how to use I2C module read external i2c sensor data, here we use veml6075 UV light sensor(color sensor)

Pin assignment:

* master:
  * GPIO2 is assigned as the clock signal of i2c master port
  * GPIO1 is assigned as the data signal of i2c master port
* Connection:
  * connect sda of sensor with GPIO1
  * connect scl of sensor with GPIO2

```c
static i2c_bus_handle_t i2c_bus = NULL;
static veml6075_handle_t veml6075 = NULL;

//Step1: Init I2C bus
i2c_config_t conf = {
    .mode = I2C_MODE_MASTER,
    .sda_io_num = VEML6075_I2C_MASTER_SDA_IO,
    .sda_pullup_en = GPIO_PULLUP_ENABLE,
    .scl_io_num = VEML6075_I2C_MASTER_SCL_IO,
    .scl_pullup_en = GPIO_PULLUP_ENABLE,
    .master.clk_speed = VEML6075_I2C_MASTER_FREQ_HZ,
};
i2c_bus = i2c_bus_create(VEML6075_I2C_MASTER_NUM, &conf);

//Step2: Init veml6075
veml6075 = veml6075_create(i2c_bus, VEML6075_I2C_ADDRESS);

//Step3: 
float uva = veml6075_get_uva(veml6075);
float uvb = veml6075_get_uvb(veml6075);
float uv_index = veml6075_get_uv_index(veml6075);
```
