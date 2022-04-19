| Supported Targets | ESP32 | ESP32-S2 | ESP32-S3 |
| ----------------- | ----- | -------- | -------- |

# ESP Camera Video Stream Server

The example starts a web server on a local network. You can view pictures dynamically through your browser.  
See the [README.md](../README.md) file in the upper level [camera](../) directory for more information about examples.  

## How to use the example


### Hardware Required

* A development board with camera module (e.g., ESP-EYE, ESP32-S2-Kaluga-1, ESP32-S3-EYE, etc.)
* A USB cable for power supply and programming

### Configure the project

step1: chose your taget chip.

````
idf.py menuconfig -> Camera Pin Configuration
````

step2: Configure your wifi.

```
idf.py menuconfig -> Example Connection Configuration
```

step3: Configure the camera.  

```
idf.py menuconfig -> component config -> Camera Configuration
```

step 4: Launch and monitor  
Flash the program and launch IDF Monitor:

```bash
idf.py flash monitor
```

step 5: Test the example interactively on a web browser (assuming IP is 192.168.43.130):  

open path `http://192.168.43.130/stream` to see an HTML web page on the server.  

**NOTE:** This example configures the output format of the camera sensor to JPEG format by default. Therefore, if your sensor does not support this function, please change this line of code:  

```
TEST_ESP_OK(init_camera(20000000, PIXFORMAT_JPEG, FRAMESIZE_QVGA, 2)); // Change PIXFORMAT_JPEG to PIXFORMAT_RGB565/PIXFORMAT_YUV422 if the sensor does not support JPEG compression.
```

**NOTE:** The current camera sensor configuration is not optimal, so don't be surprised when the picture refresh is slow. Referring to the datasheet of the camera sensor, adjusting some register parameters may improve performance.  

## Troubleshooting
* If the log shows "gpio: gpio_intr_disable(176): GPIO number error", then you probably need to check the configuration of camera pins, which you can find in the project configuration menu (`idf.py menuconfig`): Component config -> Camera Pin Config.
* If the initialization of the camera sensor fails. Please check the initialization parameters and pin configuration of your camera sensor. 
