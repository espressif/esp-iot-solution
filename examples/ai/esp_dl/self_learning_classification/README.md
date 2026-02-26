| Supported Targets | ESP32-P4 |
| ----------------- | -------- |

# Self Learning Classification Based on ESP-DL

This is a local, on-device self-learning whole-image classification example built with the [ESP-DL](https://github.com/espressif/esp-dl) framework. It does not require retraining the model. Instead, users can learn and store features offline on the device by enrolling samples from multiple objects/classes, and then classify an entire input image to determine its category. This example is best suited for fixed-view, fixed-background scenarios (where the camera viewpoint and background remain relatively stable), such as object/category recognition in a fixed workstation or a fixed capture area. For best results, keep environmental conditions stable between enrollment and recognition, including illumination/color temperature, camera exposure and white balance settings, object distance/scale, and placement angle. The model quantization and deployment process follows the approach described in [How to deploy MobileNetV2](https://docs.espressif.com/projects/esp-dl/en/latest/tutorials/how_to_deploy_mobilenetv2.html).

![self_learning_classification](https://dl.espressif.com/AE/esp-iot-solution/self_learning_classification.jpg)

Please note that in the [quantize_torch_mode script](https://github.com/espressif/esp-dl/blob/7089b94a76206825bddba57a6385d46cc08c0a6b/examples/tutorial/how_to_quantize_model/quantize_mobilenetv2/quantize_torch_model.py), the classification layer of MobileNetV2 is removed, so the model only outputs feature vectors. In the subsequent recognition process, the object's feature vector is compared with the recorded feature vectors to achieve local classification.

```python
model = torchvision.models.mobilenet.mobilenet_v2(
    weights=MobileNet_V2_Weights.IMAGENET1K_V1
)

model.classifier = torch.nn.Identity()
```

## How to use the example

### ESP-IDF Required

- This example supports ESP-IDF release/v5.5.
- Please follow the [ESP-IDF Programming Guide](https://docs.espressif.com/projects/esp-idf/en/latest/esp32p4/get-started/index.html) to set up the development environment. **We highly recommend** you [Build Your First Project](https://docs.espressif.com/projects/esp-idf/en/latest/esp32p4/get-started/index.html#build-your-first-project) to get familiar with ESP-IDF and make sure the environment is set up correctly.

### Hardware Required

* An ESP32-P4-Function-EV-Board.
* A 7-inch 1024 x 600 LCD screen powered by the [EK79007](https://dl.espressif.com/dl/schematics/display_driver_chip_EK79007AD_datasheet.pdf) IC, accompanied by a 32-pin FPC connection [adapter board](https://dl.espressif.com/dl/schematics/esp32-p4-function-ev-board-lcd-subboard-schematics.pdf) ([LCD Specifications](https://dl.espressif.com/dl/schematics/display_datasheet.pdf)).
* A MIPI-CSI camera powered by the SC2336 IC, accompanied by a 32-pin FPC connection [adapter board](https://dl.espressif.com/dl/schematics/esp32-p4-function-ev-board-camera-subboard-schematics.pdf) ([Camera Specifications](https://dl.espressif.com/dl/schematics/camera_datasheet.pdf)).
* A USB-C cable for power supply and programming.
* Please refer to the following steps for the connection:
    * **Step 1**. According to the table below, connect the pins on the back of the screen adapter board to the corresponding pins on the development board.

        | Screen Adapter Board | ESP32-P4-Function-EV-Board |
        | -------------------- | -------------------------- |
        | 5V (any one)         | 5V (any one)               |
        | GND (any one)        | GND (any one)              |
        | PWM                  | GPIO26                     |
        | LCD_RST              | GPIO27                     |

    * **Step 2**. Connect the FPC of LCD through the `MIPI_DSI` interface.
    * **Step 3**. Connect the FPC of Camera through the `MIPI_CSI` interface (SC2336).
    * **Step 4**. Use a USB-C cable to connect the `USB-UART` port to a PC (Used for power supply and viewing serial output).
    * **Step 5**. Turn on the power switch of the board.

### Build and flash

Build the project and flash it to the board, then run monitor tool to view serial output (replace `PORT` with your board's serial port name):

```c
idf.py -p PORT flash monitor
```

To exit the serial monitor, type ``Ctrl-]``.

See the [ESP-IDF Getting Started Guide](https://docs.espressif.com/projects/esp-idf/en/latest/get-started/index.html) for full steps to configure and use ESP-IDF to build projects.

## Performance

The following latency numbers are measured on this example:

- Average class enrollment time: 750 ms
- Average distance computation time: 1 ms

If faster inference is required, you can also replace MobileNetV2 with other lightweight classification/backbone models to further reduce end-to-end latency.

## Example output

In this example, 5 classes are reserved by default for recognition. During recognition, the predicted category is accepted only when confidence is greater than or equal to `80%` (default, defined in `main/self_learning_classifier.cpp`); otherwise, the result is treated as no valid class match (`class_id = -1`). After selecting a class, click the `Record` button to record samples. Click `Recog` to start recognition. The more samples recorded for each class, the higher the recognition accuracy.


```shell
(29) boot: ESP-IDF v5.5.2-737-g0da66850d91 2nd stage bootloader
I (29) boot: compile time Feb 26 2026 10:09:49
I (29) boot: Multicore bootloader
I (32) boot: chip revision: v1.0
I (33) boot: efuse block revision: v0.3
I (37) boot.esp32p4: SPI Speed      : 40MHz
I (41) boot.esp32p4: SPI Mode       : DIO
I (45) boot.esp32p4: SPI Flash Size : 16MB
I (48) boot: Enabling RNG early entropy source...
I (53) boot: Partition Table:
I (56) boot: ## Label            Usage          Type ST Offset   Length
I (62) boot:  0 nvs              WiFi data        01 02 0000a000 00006000
I (68) boot:  1 phy_init         RF data          01 01 00010000 00001000
I (75) boot:  2 factory          factory app      00 00 00020000 007d0000
I (83) boot: End of partition table
I (85) esp_image: segment 0: paddr=00020020 vaddr=481d0020 size=2726bch (2565820) map
I (534) esp_image: segment 1: paddr=002926e4 vaddr=30100000 size=00068h (   104) load
I (536) esp_image: segment 2: paddr=00292754 vaddr=4ff00000 size=0d8c4h ( 55492) load
I (550) esp_image: segment 3: paddr=002a0020 vaddr=48000020 size=1c53e0h (1856480) map
I (870) esp_image: segment 4: paddr=00465408 vaddr=4ff0d8c4 size=09d40h ( 40256) load
I (880) esp_image: segment 5: paddr=0046f150 vaddr=4ff17680 size=040bch ( 16572) load
I (884) esp_image: segment 6: paddr=00473214 vaddr=50108080 size=00020h (    32) load
I (891) boot: Loaded app from partition at offset 0x20000
I (891) boot: Disabling RNG early entropy source...
I (907) hex_psram: vendor id    : 0x0d (AP)
I (907) hex_psram: Latency      : 0x01 (Fixed)
I (907) hex_psram: DriveStr.    : 0x00 (25 Ohm)
I (908) hex_psram: dev id       : 0x03 (generation 4)
I (912) hex_psram: density      : 0x07 (256 Mbit)
I (917) hex_psram: good-die     : 0x06 (Pass)
I (921) hex_psram: SRF          : 0x02 (Slow Refresh)
I (926) hex_psram: BurstType    : 0x00 ( Wrap)
I (930) hex_psram: BurstLen     : 0x03 (2048 Byte)
I (934) hex_psram: BitMode      : 0x01 (X16 Mode)
I (939) hex_psram: Readlatency  : 0x04 (14 cycles@Fixed)
I (944) hex_psram: DriveStrength: 0x00 (1/1)
I (948) MSPI Timing: Enter psram timing tuning
I esp_psram: Found 32MB PSRAM device
I esp_psram: Speed: 200MHz
I (1450) mmu_psram: .rodata xip on psram
I (1685) mmu_psram: .text xip on psram
I (1686) hex_psram: psram CS IO is dedicated
I (1686) cpu_start: Multicore app
I (2131) esp_psram: SPI SRAM memory test OK
I (2140) cpu_start: GPIO 38 and 37 are used as console UART I/O pins
I (2140) cpu_start: Pro cpu start user code
I (2140) cpu_start: cpu freq: 360000000 Hz
I (2142) app_init: Application information:
I (2146) app_init: Project name:     self_learning_classification
I (2152) app_init: App version:      824bcb8db-dirty
I (2157) app_init: Compile time:     Feb 26 2026 10:54:02
I (2162) app_init: ELF file SHA256:  378891e94...
I (2166) app_init: ESP-IDF:          v5.5.2-737-g0da66850d91
I (2172) efuse_init: Min chip rev:     v0.0
I (2176) efuse_init: Max chip rev:     v1.99 
I (2180) efuse_init: Chip rev:         v1.0
I (2184) heap_init: Initializing. RAM available for dynamic allocation:
I (2190) heap_init: At 4FF1CAC0 len 0001E500 (121 KiB): RETENT_RAM
I (2196) heap_init: At 4FF3AFC0 len 00004BF0 (18 KiB): RAM
I (2201) heap_init: At 4FF50DB0 len 0002F250 (188 KiB): RAM
I (2207) heap_init: At 501080A0 len 00007F60 (31 KiB): RTCRAM
I (2212) heap_init: At 30100068 len 00001F98 (7 KiB): TCM
I (2217) esp_psram: Adding pool of 28352K of PSRAM memory to heap allocator
I (2224) esp_psram: Adding pool of 43K of PSRAM memory gap generated due to end address alignment of irom to the heap allocator
I (2235) esp_psram: Adding pool of 54K of PSRAM memory gap generated due to end address alignment of drom to the heap allocator
I (2246) spi_flash: detected chip: gd
I (2249) spi_flash: flash io: dio
I (2253) sleep_gpio: Configure to isolate all GPIO pins in sleep state
I (2259) sleep_gpio: Enable automatic switching of GPIO sleep configuration
I (2266) main_task: Started on CPU0
I (2270) esp_psram: Reserving pool of 32K of internal memory for DMA/internal allocations
I (2277) main_task: Calling app_main()
I (2280) LVGL: Starting LVGL task
W (2281) ledc: GPIO 26 is not usable, maybe conflict with others
I (2289) ESP32_P4_EV: MIPI DSI PHY Powered on
I (2294) ESP32_P4_EV: Install MIPI DSI LCD control panel
I (2298) ESP32_P4_EV: Install EK79007 LCD control panel
I (2303) ek79007: version: 1.0.4
I (2476) ESP32_P4_EV: Display initialized
I (2476) ESP32_P4_EV: Display resolution 1024x600
E (2477) lcd_panel: esp_lcd_panel_swap_xy(50): swap_xy is not supported by this panel
W (2480) i2c.master: Please check pull-up resistances whether be connected properly. Otherwise unexpected behavior would happen. For more detailed information, please read docs
I (2496) GT911: I2C address initialization procedure skipped - using default GT9xx setup
I (2504) GT911: TouchPad_ID:0x39,0x31,0x31
I (2507) GT911: TouchPad_Config_Version:89
I (2511) ESP32_P4_EV: Setting LCD backlight: 100%
I (2516) ESP32_P4_EV: Setting LCD backlight: 100%
I (2526) sc2336: Detected Camera sensor PID=0xcb3a
I (2605) cam: version: 1.4.1
I (2605) cam: driver:  MIPI-CSI
I (2605) cam: card:    MIPI-CSI
I (2605) cam: bus:     esp32p4:MIPI-CSI
I (2606) cam: Width=1280 Height=720
W (2610) ISP_AWB: Subwindow feature is not supported on REV < 3.0, subwindow will not be configured
W (2619) FbsLoader: There is only one model in the flatbuffers, ignore the input model name!
W (2626) FbsLoader: CONFIG_SPIRAM_RODATA or CONFIG_SPIRAM_XIP_FROM_PSRAM option is on, fbs model is copied to PSRAM.
W (2701) dl::Model: Minimize() will delete variables not used in model inference, which will make it impossible to test or debug the model.
I (2703) self_learning: SelfLearningClassifier created, feature dim: 1280, max prototypes/class: 7
I (2712) main_task: Returned from app_main()
I (7869) self_learning: Created new class 0
I (7869) self_learning: Enrolled to class 0: 1/7 prototypes, 1 total samples, avg_intra_dist=0.0000
I (7870) app_classifier: Enrolled to class 0
I (9384) self_learning: Enrolled to class 0: 2/7 prototypes, 2 total samples, avg_intra_dist=0.3002
I (9385) app_classifier: Enrolled to class 0
I (10791) self_learning: Enrolled to class 0: 3/7 prototypes, 3 total samples, avg_intra_dist=0.4023
I (10791) app_classifier: Enrolled to class 0
I (12618) self_learning: Enrolled to class 0: 4/7 prototypes, 4 total samples, avg_intra_dist=0.4716
I (12619) app_classifier: Enrolled to class 0
I (14301) self_learning: Enrolled to class 0: 5/7 prototypes, 5 total samples, avg_intra_dist=0.4754
I (14302) app_classifier: Enrolled to class 0
I (16327) self_learning: Enrolled to class 0: 6/7 prototypes, 6 total samples, avg_intra_dist=0.4980
I (16328) app_classifier: Enrolled to class 0
I (17778) self_learning: Enrolled to class 0: 7/7 prototypes, 7 total samples, avg_intra_dist=0.5137
I (17779) app_classifier: Enrolled to class 0
I (20462) self_learning: Enrolled to class 0: 7/7 prototypes, 8 total samples, avg_intra_dist=0.5372
I (20462) app_classifier: Enrolled to class 0
I (21941) self_learning: Enrolled to class 0: 7/7 prototypes, 9 total samples, avg_intra_dist=0.5716
I (21942) app_classifier: Enrolled to class 0
I (26591) self_learning: Created new class 1
I (26592) self_learning: Enrolled to class 1: 1/7 prototypes, 1 total samples, avg_intra_dist=0.0000
I (26593) app_classifier: Enrolled to class 1
I (28080) self_learning: Enrolled to class 1: 2/7 prototypes, 2 total samples, avg_intra_dist=0.2860
I (28080) app_classifier: Enrolled to class 1
I (29529) self_learning: Enrolled to class 1: 3/7 prototypes, 3 total samples, avg_intra_dist=0.3901
I (29529) app_classifier: Enrolled to class 1
I (30880) self_learning: Enrolled to class 1: 4/7 prototypes, 4 total samples, avg_intra_dist=0.4003
I (30880) app_classifier: Enrolled to class 1
I (32230) self_learning: Enrolled to class 1: 5/7 prototypes, 5 total samples, avg_intra_dist=0.3723
I (32230) app_classifier: Enrolled to class 1
I (36767) self_learning: Enrolled to class 1: 6/7 prototypes, 6 total samples, avg_intra_dist=0.4751
I (36768) app_classifier: Enrolled to class 1
I (38398) self_learning: Enrolled to class 1: 7/7 prototypes, 7 total samples, avg_intra_dist=0.5275
I (38398) app_classifier: Enrolled to class 1
I (40195) self_learning: Enrolled to class 1: 7/7 prototypes, 8 total samples, avg_intra_dist=0.5136
I (40196) app_classifier: Enrolled to class 1
I (41444) self_learning: Enrolled to class 1: 7/7 prototypes, 9 total samples, avg_intra_dist=0.5051
I (41444) app_classifier: Enrolled to class 1
I (42464) self_learning: Predict: best_class=1, dist=0.3092, confidence=95.2%
```

![self_learning_classification_example](https://dl.espressif.com/AE/esp-iot-solution/self_learning_classification_example.gif)
