[![Component Registry](https://components.espressif.com/components/espressif/touch_proximity_sensor/badge.svg)](https://components.espressif.com/components/espressif/touch_proximity_sensor)

| Supported Targets | ESP32-S3 |
| ----------------- | -------- |

# Component: touch_proximity_sensor
This component includes a set of easy-to-use APIs for creating, starting, stopping, and deleting touch proximity sensors. With these APIs, developers can easily implement proximity detection functionality, suitable for various contactless sensing scenarios.

Proximity uses the low-level accumulated measurement path and a single
multi-frequency FSM. Configure `touch_proxi_config_t.thresholds` in normalized
FSM decision counts measured on the target. These thresholds are independent
of ordinary touch raw counts and are not converted from a presumed 5000-count
baseline. The effective FSM frame period includes `proximity_count`; startup
and debounce timing therefore follows the measured accumulated proximity
frame period reported by the wrapper.

> Note:
1. ESP32/ESP32-S2/ESP32-S3 touch-related components are intended for testing or demo purposes only. Due to the poor anti-interference capability of the touch functionality, it may not pass EMS testing, and therefore, it is not recommended for mass production products.
2. This component is currently only applicable to the ESP32S3 and requires an IDF version greater than or equal to v5.5.

### Add component to your project

Please use the component manager command `add-dependency` to add the `touch_proximity_sensor` to your project's dependency, during the `CMake` step the component will be downloaded automatically

```
idf.py add-dependency "espressif/touch_proximity_sensor=*"
```
