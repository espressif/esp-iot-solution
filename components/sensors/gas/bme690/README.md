# BME69X SensorAPI

> **Bosch Sensortec's [BME690](https://www.bosch-sensortec.com/products/environmental-sensors/gas-sensors/bme690/) SensorAPI**
>
> This driver component is based on the Bosch Sensortec's [BME690_SensorAPI](https://github.com/boschsensortec/BME690_SensorAPI) v1.0.3 [`eed112e4`](https://github.com/boschsensortec/BME690_SensorAPI/tree/eed112e49c5a4a9bd104fd952aa15c00f1fb265a),
> with modifications to `common/*` to adapt to the ESP platform.
>
> See [bme69x.h](./bme69x.h) and [common/common.h](./common/common.h) for API usage.

## Sensor Overview
BME690 is an integrated environmental sensor developed specifically for mobile applications and wearables where size and low power consumption are key requirements. Expanding Bosch Sensortec’s existing family of environmental sensors, the BME690 integrates for the first time high-linearity and high-accuracy gas, pressure, humidity and temperature sensors. It consists of an 8-pin metal-lid 3.0 x 3.0 x 0.93 mm³ LGA package which is designed for optimized consumption depending on the specific operating mode, long term stability and high EMC robustness. The gas sensor within the BME690 can detect a broad range of gases to measure air quality for personal well being. Gases that can be detected by the BME690 include Volatile Organic Compounds (VOC) from paints (such as formaldehyde), lacquers, paint strippers, cleaning supplies, furnishings, office equipment, glues, adhesives and alcohol.

### Features

- Air quality measurement
- Personalized weather station
- Context awareness, e.g. skin moisture detection, room change detection
- Fitness monitoring / well-being
- Warning regarding dryness or high temperatures
- Measurement of volume and air flow
- Home automation control (e.g. HVAC)
- GPS enhancement (e.g. time-to-first-fix improvement, dead reckoning, slope detection)
- Indoor navigation (change of floor detection, elevator detection)
- Altitude tracking and calories expenditure for sports activities

---
