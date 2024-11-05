# Component: VEML6040

VEML6040 color sensor senses red, green, blue, and white light and incorporates photodiodes, amplifiers, and analog / digital circuits into a single chip using CMOS process. With the color sensor applied, the brightness, and color temperature of backlight can be adjusted base on ambient light source that makes panel looks more comfortable for end user’s eyes.

## Add component to your project

Please use the component manager command `add-dependency` to add the `veml6040` to your project's dependency, during the `CMake` step the component will be downloaded automatically

```
idf.py add-dependency "espressif/veml6040=*"
```

## Example of VEML6040 usage


* This component will show you how to use I2C module read external i2c sensor data, here we use veml6040 UV light sensor(color sensor)

Pin assignment:

* master:
  * GPIO2 is assigned as the clock signal of i2c master port
  * GPIO1 is assigned as the data signal of i2c master port
* Connection:
  * connect sda of sensor with GPIO1
  * connect scl of sensor with GPIO2

```c
static i2c_bus_handle_t i2c_bus = NULL;
static veml6040_handle_t veml6040 = NULL;

//Step1: Init I2C bus
i2c_config_t conf = {
    .mode = I2C_MODE_MASTER,
    .sda_io_num = VEML6040_I2C_MASTER_SDA_IO,
    .sda_pullup_en = GPIO_PULLUP_ENABLE,
    .scl_io_num = VEML6040_I2C_MASTER_SCL_IO,
    .scl_pullup_en = GPIO_PULLUP_ENABLE,
    .master.clk_speed = VEML6040_I2C_MASTER_FREQ_HZ,
};
i2c_bus = i2c_bus_create(VEML6040_I2C_MASTER_NUM, &conf);

//Step2: Init veml6040
veml6040 = veml6040_create(i2c_bus, VEML6040_I2C_ADDRESS);

//Step3: Read red, green, blue and lux
int red = veml6040_get_red(veml6040);
int green = veml6040_get_green(veml6040)
int blue = veml6040_get_blue(veml6040)
int lux = veml6040_get_lux(veml6040);
```
