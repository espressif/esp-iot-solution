# BQ27220

The [BQ27220](https://www.ti.com/product/BQ27220) is a single-cell Li-Ion battery fuel gauge IC that provides precise battery status monitoring, including state of charge (SoC), remaining capacity, state of health (SoH), and other critical parameters. This component provides a driver implementation for the BQ27220 in embedded systems, supporting communication via the I2C interface with the host controller. 

In addition to the basic functionality, the component also include a tool [sample-data-app](tools/sample-data-app), which can be used to monitor the battery status in real-time and print data with CSV format for Gauging Parameter Calculator.

## Example of usage

``` c

#define I2C_MASTER_SCL_IO          GPIO_NUM_1         /*!< gpio number for I2C master clock */
#define I2C_MASTER_SDA_IO          GPIO_NUM_2         /*!< gpio number for I2C master data  */

// Default Gauging Parameter
static const parameter_cedv_t default_cedv = {
    .full_charge_cap = 650,
    .design_cap = 650,
    .reserve_cap = 0,
    .near_full = 200,
    .self_discharge_rate = 20,
    .EDV0 = 3490,
    .EDV1 = 3511,
    .EDV2 = 3535,
    .EMF = 3670,
    .C0 = 115,
    .R0 = 968,
    .T0 = 4547,
    .R1 = 4764,
    .TC = 11,
    .C1 = 0,
    .DOD0 = 4147,
    .DOD10 = 4002,
    .DOD20 = 3969,
    .DOD30 = 3938,
    .DOD40 = 3880,
    .DOD50 = 3824,
    .DOD60 = 3794,
    .DOD70 = 3753,
    .DOD80 = 3677,
    .DOD90 = 3574,
    .DOD100 = 3490,
};

// Default Gauging Config
static const gauging_config_t default_config = {
    .CCT = 1,
    .CSYNC = 0,
    .EDV_CMP = 0,
    .SC = 1,
    .FIXED_EDV0 = 0,
    .FCC_LIM = 1,
    .FC_FOR_VDQ = 1,
    .IGNORE_SD = 1,
    .SME0 = 0,
};

static i2c_bus_handle_t i2c_bus = NULL;
static bq27220_handle_t bq27220 = NULL;

i2c_config_t conf = {
    .mode = I2C_MODE_MASTER,
    .sda_io_num = I2C_MASTER_SDA_IO,
    .sda_pullup_en = GPIO_PULLUP_ENABLE,
    .scl_io_num = I2C_MASTER_SCL_IO,
    .scl_pullup_en = GPIO_PULLUP_ENABLE,
    .master.clk_speed = 400 * 1000,
};
i2c_bus = i2c_bus_create(I2C_NUM_0, &conf);

bq27220_config_t bq27220_cfg = {
    .i2c_bus = i2c_bus,
    .cfg = &default_config,
    .cedv = &default_cedv,
};
bq27220 = bq27220_create(&bq27220_cfg);

```
