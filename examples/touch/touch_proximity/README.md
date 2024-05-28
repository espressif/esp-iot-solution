## Touch Proximity sensor example

This example demonstrates how to develop a touch proximity sensor application using the touch_proximity_sensor component on the ESP32-S3. When the hand is within a certain threshold distance from the sensor board, the application will trigger the buzzer to sound an alarm, indicating that proximity has been detected. This example can be applied to various contactless proximity sensing scenarios, such as contactless doorbells.

> Note: The example is for testing and demonstration purposes only. Due to the poor anti-interference capability of the touch functionality, it may not pass EMS testing, and therefore, it is not recommended for mass production products.

## How to Use This Example

### Hardware

This example uses the ESP32-S2S3-Touch-DevKit-1 MainBoard v1.1 and the touch sub-board ESP32-S2S3-Touch-DevKit-1 Proximity Board V1.0. For more details, please refer to the [reference documentation](https://docs.espressif.com/projects/espressif-esp-dev-kits/zh_CN/latest/esp32s2/esp32-s2-touch-devkit-1/user_guide.html)。

### Build and Flash

Use the idf.py tool to build and flash the program with the following command:

```
idf.py -p PORT build flash
```
Replace PORT with the port number you are using.


### Example Output

```
I (117) main_task: Started on CPU0
I (118) main_task: Calling app_main()
I (119) gpio: GPIO[15]| InputEn: 0| OutputEn: 1| OpenDrain: 0| Pullup: 0| Pulldown: 0| Intr:0 
I (120) touch-proximity-sensor: IoT Touch Proximity Driver Version: 0.1.1
I (161) touch-proximity-sensor: proxi daemon task start!
I (361) touch-proximity-example: touch proximity sensor has started! when you approach the touch sub-board, the buzzer will sound.
I (361) main_task: Returned from app_main()
I (2406) touch-proximity-example: CH8, active!
I (2646) touch-proximity-example: CH8, inactive!
I (3568) touch-proximity-example: CH8, active!
I (4177) touch-proximity-example: CH8, inactive!
I (4767) touch-proximity-example: CH8, active!
I (5561) touch-proximity-example: CH8, inactive!
I (6096) touch-proximity-example: CH8, active!
I (7277) touch-proximity-example: CH8, inactive!
```
