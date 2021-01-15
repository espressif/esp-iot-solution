
## Sensors Driver

### 1. Humiture

|Name|Function| Bus |Vendor|Datasheet|
|--|--|--|--|--|
|HDC2010 |temperature，humidity|I2C|TI|[datasheet](https://www.ti.com/lit/gpn/hdc2010)|
|HTS221 |temperature，humidity|I2C|ST|[datasheet](https://www.st.com/resource/en/datasheet/hts221.pdf)|
|SHT3X |temperature，humidity|I2C|Sensirion|[datasheet](https://www.mouser.com/datasheet/2/682/Sensirion_Humidity_Sensors_SHT3x_Datasheet_digital-971521.pdf)|
|MVH3004D |temperature，humidity|I2C|--|--|

### 2. Light Sensor

|Name|Function| Bus |Vendor|Datasheet|
|--|--|--|--|--|
|APDS9960|Light, RGB and Gesture Sensor|I2C|Avago|[datesheet](https://cdn.sparkfun.com/assets/learn_tutorials/3/2/1/Avago-APDS-9960-datasheet.pdf)|
|BH1750|Light|I2C|rohm|[datasheet](https://www.mouser.com/datasheet/2/348/bh1750fvi-e-186247.pdf)|
|VEML6040|Light RGBW|I2C|Vishay|[datasheet](https://www.vishay.com/docs/84276/veml6040.pdf)|
|VEML6075|Light UVA UVB|I2C|Vishay|[datasheet](https://cdn.sparkfun.com/assets/3/c/3/2/f/veml6075.pdf)|

### 3. Pressure

|Name|Function| Bus |Vendor|Datasheet|
|--|--|--|--|--|
|BME280| Pressure |I2C/SPI | BOSCH|[datesheet](https://ae-bst.resource.bosch.com/media/_tech/media/datasheets/BST-BME280-DS002.pdf) |

### 4. IMU (Inertial Measurement Unit)

|Name|Function| Bus |Vendor|Datasheet|
|--|--|--|--|--|
|LIS2DH12 |3-axis acceler | I2C |ST|[datasheet](https://www.st.com/resource/en/datasheet/lis2dh12.pdf)|
|MPU6050 |3-axis acceler + 3-axis gyro | I2C|InvenSense|[datasheet](https://invensense.tdk.com/wp-content/uploads/2015/02/MPU-6000-Datasheet1.pdf)|

## Sensors Hal

|  Type Name   |          Function          |     Supported Sensors      |
| :----------: | :------------------------: | :------------------------: |
|   Humiture   |   temperature 、humidity   |       HTS221、SHT3X        |
| Light Sensor | Light intensity、color、uv | BH1750、VEML6040、VEML6075 |
|     IMU      |       acceler、gyro        |     LIS2DH12、MPU6050      |
