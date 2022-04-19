| Supported Targets | ESP32 | ESP32-S2 | ESP32-S3 |
| ----------------- | ----- | -------- | -------- |
# ESP Camera Sensor Frame Rate Test

This example is used to test the rate of images obtained from the camera sensor.  
See the [README.md](../README.md) file in the upper level [camera](../) directory for more information about examples.  

## Note about frame rate
The frame rate can be measured by how many images are obtained in 1s. This example estimates the rate by obtaining a certain number of pictures and testing the time to obtain these pictures.  

fps(frames/sec) = `picture_count` / `time_consumed`.  

The main factors affecting the camera frame rate are：  

| name         |  definition   |
| ------------ | ---- |
|          xclk          |Clock frequency of driving camera sensor. Generally, increasing this clock frequency will improve the frame rate.|
| image format | Common output formats: RGB565/YUV422/JPEG. The data in JPEG format is compressed data, so selecting the image data in JPEG format can usually improve the frame rate. |
| image size   | Image resolution. Larger resolution means that more data needs to be transmitted per unit time. Therefore, the higher the resolution, the lower the frame rate may be. |
| fb_count     | The number of buffers used to receive the data transmitted by the sensor. Increasing this value may improve the frame rate. |
| jpeg_quality     | Lower JPEG quality means less data needs to be transmitted, so it may help to improve the frame rate. |

All the above factors can be specified in the `esp_camera_init()`. You can try to change them and test their impact on the frame rate. 
## How to use the example

### Hardware Required

* A development board with camera module (e.g., ESP-EYE, ESP32-S2-Kaluga-1, ESP32-S3-EYE, etc.)
* A USB cable for power supply and programming

### Configure the project

step1: chose your taget chip.

````
idf.py menuconfig -> Camera Pin Configuration
````
step2: Configure the camera.
```
idf.py menuconfig ->component config -> Camera Configuration
```

### Build and Flash

Build the project and flash it to the board, then run monitor tool to view serial output:

```
idf.py -p PORT flash monitor
```

(Replace PORT with the name of the serial port to use.)

(To exit the serial monitor, type ``Ctrl-]``.)

See the [Getting Started Guide](https://docs.espressif.com/projects/esp-idf/en/latest/get-started/index.html) for full steps to configure and use ESP-IDF to build projects.

## Troubleshooting
* If the log shows "gpio: gpio_intr_disable(176): GPIO number error", then you probably need to check the configuration of camera pins, which you can find in the project configuration menu (`idf.py menuconfig`): Component config -> Camera Pin Config.
* If the initialization of the camera sensor fails. Please check the initialization parameters and pin configuration of your camera sensor. 