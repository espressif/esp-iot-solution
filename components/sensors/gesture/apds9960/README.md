# Component: APDS9960

The APDS9960 device features advanced Gesture detection, Proximity detection, Digital Ambient Light Sense (ALS) and Color Sense (RGBC). The slim modular package, L 3.94 x W 2.36 x H 1.35 mm, incorporates an IR LED and factory calibrated LED driver for drop-in compatibility with existing footprints.

## Add component to your project

Please use the component manager command `add-dependency` to add the `apds9960` to your project's dependency, during the `CMake` step the component will be downloaded automatically

```
idf.py add-dependency "espressif/apds9960=*"
```

## Example of APDS9960 usage

Pin assignment:

* master:
  * GPIO3 is assigned as VL IO
  * GPIO4 is assigned as the clock signal of i2c master port
  * GPIO5 is assigned as the data signal of i2c master port
* Connection:
  * connect sda of sensor with GPIO5
  * connect scl of sensor with GPIO4
  * connect vl of sensor with GPIO3

```c
i2c_bus_handle_t i2c_bus = NULL;
apds9960_handle_t apds9960 = NULL;

//Step1: Init I2C bus and VL io
gpio_config_t cfg;
cfg.pin_bit_mask = BIT(APDS9960_VL_IO);
cfg.intr_type = 0;
cfg.mode = GPIO_MODE_OUTPUT;
cfg.pull_down_en = 0;
cfg.pull_up_en = 0;
gpio_config(&cfg);
gpio_set_level(APDS9960_VL_IO, 0);

int i2c_master_port = APDS9960_I2C_MASTER_NUM;
i2c_config_t conf = {
    .mode = I2C_MODE_MASTER,
    .sda_io_num = APDS9960_I2C_MASTER_SDA_IO,
    .sda_pullup_en = GPIO_PULLUP_ENABLE,
    .scl_io_num = APDS9960_I2C_MASTER_SCL_IO,
    .scl_pullup_en = GPIO_PULLUP_ENABLE,
    .master.clk_speed = APDS9960_I2C_MASTER_FREQ_HZ,
};
i2c_bus = i2c_bus_create(i2c_master_port, &conf);

//Step2: Init apds9960
apds9960 = apds9960_create(i2c_bus, APDS9960_I2C_ADDRESS);

//Step3: Read gesture
uint8_t gesture = apds9960_read_gesture(apds9960);
```
