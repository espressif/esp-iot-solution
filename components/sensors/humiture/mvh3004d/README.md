# Component: MVH3004D


## Add component to your project

Please use the component manager command `add-dependency` to add the `mvh3004d` to your project's dependency, during the `CMake` step the component will be downloaded automatically

```
idf.py add-dependency "espressif/mvh3004d=*"
```

## Example of MVH3004D usage


Pin assignment:

* master:
  * GPIO2 is assigned as the clock signal of i2c master port
  * GPIO1 is assigned as the data signal of i2c master port
* Connection:
  * connect sda of sensor with GPIO1
  * connect scl of sensor with GPIO2

```c
static i2c_bus_handle_t i2c_bus = NULL;
static mvh3004d_handle_t mvh3004d = NULL;

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

//Step2: Init mvh3004d
mvh3004d = mvh3004d_create(i2c_bus, MVH3004D_SLAVE_ADDR);

//Step3: Read tp and rh
float tp = 0.0, rh = 0.0;
mvh3004d_get_data(mvh3004d, &tp, &rh);
```
