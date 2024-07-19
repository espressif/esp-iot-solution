# Component: DRV10987

The DRV10987 device is a 3-phase sensorless 180° sinusodial motor driver with integrated power MOSFETs, which can provide continuous drive current up to 2 A.

* This component will show you how to read and write DRV10987.
* Pin assignment:
* master:
  * GPIO23 is assigned as the data signal of i2c master port.
  * GPIO22 is assigned as the clock signal of i2c master port.
  * GPIO21 is assigned as rotation direction control.
* connection:
  * Connect sda of drv10987 with GPIO23.
  * Connect scl of drv10987 with GPIO22.
  * Connect dir of drv10987 with GPIO21.


## Startup process

It is recommended to program the EEPROM with the motor off and read back the EEPROM to verify that the drive is valid.

1. Power up with any voltage within operating voltage range (6.2 V to 28 V).
2. Wait 10 ms.
3. Write register 0x60 to set MTR_DIS = 1; this disables the motor driver.
4. Write register 0x31 with 0x0000 to clear the EEPROM access code.
5. Write register 0x31 with 0xC0DE to enable access to EEPROM.
7. Read register 0x32 for eeReadyStatus = 1.
8. Case-A: Mass Write:
   1. Write all individual shadow registers.
      1. Write register 0x90 (CONFIG1) with CONFIG1 data.
      2. ...
      3. Write register 0x96 (CONFIG7) with CONFIG7 data.
   2. Write the following to register 0x35.
      1. ShadowRegEn = 0
      2. eeRefresh = 0
      3. eeWRnEn = 1
      4. EEPROM Access Mode = 10
   3. Wait for register 0x32 eeReadyStatus = 1 – EEPROM is now updated with the contents of the shadow registers.
9. Case-B: Mass Read：
   1. Write the following to register 0x35.
      1. ShadowRegEn = 0
      2. eeRefresh = 0
      3. eeWRnEn =0
      4. eeAccMode = 10
   2.  Internally, the device starts reading the EEPROM and storing it in the shadow registers.
   3.  Wait for register 0x32 eeReadyStatus = 1 – shadow registers now contain the EEPROM values.
10. Write register 0x60 to set MTR_DIS = 0; this re-enables the motor driver.

More detailed data can be found in the [DRV10987 Datasheet](https://www.ti.com/lit/gpn/drv10987).

## Example of DRV10987 usage

The `drv10987` component provides a simplified encapsulation of the above process, enabling users to drive drv10987 faster.

It is worth noting that the `drv10987` component provides default configuration entries for the configuration registers, but for different motors it is necessary to **modify the rm and kt** of the default configuration entries for config1 and config2.

```c
static i2c_bus_handle_t i2c_bus = NULL;
static drv10987_handle_t drv10987 = NULL;

//Step1：Init I2C bus
i2c_config_t conf = {
   .mode = I2C_MODE_MASTER,
   .sda_io_num = I2C_MASTER_SDA_IO,
   .sda_pullup_en = GPIO_PULLUP_ENABLE,
   .scl_io_num = I2C_MASTER_SCL_IO,
   .scl_pullup_en = GPIO_PULLUP_ENABLE,
   .master.clk_speed = I2C_MASTER_FREQ_HZ,
};
i2c_bus = i2c_bus_create(I2C_MASTER_NUM, &conf);

//Step2：Init drv10987
drv10987_config_t drv10987_cfg = {
   .bus = i2c_bus,
   .dev_addr = DRV10987_I2C_ADDRESS_DEFAULT,
   .dir_pin = DIRECTION_IO,
};
esp_err_t err = drv10987_create(&drv10987, &drv10987_cfg);

//Step3：Set the direction of motor rotation
drv10987_set_direction_uvw(drv10987);

//Step4：Enable access to EEPROM and check if EEPROM is ready for read/write access.
drv10987_write_access_to_eeprom(drv10987, 5);

//Step5：Write configuration to EEPROM
//The component provides a default configuration, for different brushless motors it is necessary to configure Rm and Kt in config1 and config2.
drv10987_config1_t config1 = DEFAULT_DRV10987_CONFIG1;
drv10987_write_config_register(drv10987, CONFIG1_REGISTER_ADDR, &config1);
drv10987_config2_t config2 = DEFAULT_DRV10987_CONFIG2;
drv10987_write_config_register(drv10987, CONFIG2_REGISTER_ADDR, &config2);
drv10987_config3_t config3 = DEFAULT_DRV10987_CONFIG3;
drv10987_write_config_register(drv10987, CONFIG3_REGISTER_ADDR, &config3);
drv10987_config4_t config4 = DEFAULT_DRV10987_CONFIG4;
drv10987_write_config_register(drv10987, CONFIG4_REGISTER_ADDR, &config4);
drv10987_config5_t config5 = DEFAULT_DRV10987_CONFIG5;
drv10987_write_config_register(drv10987, CONFIG5_REGISTER_ADDR, &config5);
drv10987_config6_t config6 = DEFAULT_DRV10987_CONFIG6;
drv10987_write_config_register(drv10987, CONFIG6_REGISTER_ADDR, &config6);
drv10987_config7_t config7 = DEFAULT_DRV10987_CONFIG7;
drv10987_write_config_register(drv10987, CONFIG7_REGISTER_ADDR, &config7);
drv10987_mass_write_acess_to_eeprom(drv10987);

//Step6：Waiting for eeprom status to be ready
while (1) {
   if (drv10987_read_eeprom_status(drv10987) == EEPROM_READY) {
      break;
   }
}

//Step7 Write motor speed and start drv10987
drv10987_enable_control(drv10987);
drv10987_write_speed(drv10987, CONFIG_EXAMPLE_MOROT_SPEED);
drv10987_enable_control(drv10987);
```