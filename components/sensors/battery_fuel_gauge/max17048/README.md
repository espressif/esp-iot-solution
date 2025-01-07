# MAX17048/MAX17049

The MAX17048/MAX17049 ICs are tiny, micropower current fuel gauges for lithium-ion (Li+) batteries in handheld and portable equipment. The MAX17048 operates with a single lithium cell and the MAX17049 with two lithium cells in series. The ICs use the sophisticated Li+ battery-modeling algorithm ModelGauge™ to track the battery relative state-of-charge (SOC) continuously over widely varying charge and discharge conditions. The ModelGauge algorithm eliminates current-sense resistor and battery-learn cycles required in traditional fuel gauges. Temperature compensation is implemented using the system microcontroller.

More detailed data can be found in the [MAX17048/MAX17049 Datasheet](https://www.analog.com/media/en/technical-documentation/data-sheets/MAX17048-MAX17049.pdf).

## Example of MAX17048/MAX17049 usage

```c
i2c_bus_handle_t i2c_bus = NULL;
max17048_handle_t max17048 = NULL;

//Step1: Init I2C bus
i2c_config_t conf = {
    .mode = I2C_MODE_MASTER,
    .sda_io_num = I2C_MASTER_SDA_IO,
    .sda_pullup_en = GPIO_PULLUP_ENABLE,
    .scl_io_num = I2C_MASTER_SCL_IO,
    .scl_pullup_en = GPIO_PULLUP_ENABLE,
    .master.clk_speed = 400 * 1000,
};
i2c_bus = i2c_bus_create(I2C_NUM_0, &conf);

//Step2: Init max17048
max17048 = max17048_create(i2c_bus, MAX17048_I2CADDR_DEFAULT);

// Step2: Get voltage and battery percentage
float voltage = 0, percent = 0;
max17048_get_cell_voltage(max17048, &voltage);
max17048_get_cell_percent(max17048, &percent);
```
