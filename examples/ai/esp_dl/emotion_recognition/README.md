| Supported Targets | ESP32-P4 | ESP32-S31 |
| ----------------- | -------- | --------- |

# Emotion Recognition Based on ESP-DL

This demo deploys a 7-class facial emotion classifier on ESP32-P4 and ESP32-S31 (both reuse the same ESP32-P4 model). The device streams from a camera, runs inference on the captured frame, and displays the predicted emotion on the LCD. The model is trained in PyTorch on a PC, quantized with ESP-PPQ (mixed INT8 / INT16), and the resulting `.espdl` file is flashed into the chip.

## Model

| Item | Value |
|---|---|
| Dataset | **RAF-DB** (Real-world Affective Faces Database) |
| Train split | 12,271 pre-aligned face images across 7 classes |
| Valid split | 3,068 images |
| Backbone | **MobileNetV2** (ImageNet pretrained, fine-tuned) |
| Classifier head | `Linear(1280 → 256) → ReLU6 → Dropout(0.3) → Linear(256 → 7)` |
| Input | 112 × 112 RGB |
| Normalization | ImageNet: mean=[0.485, 0.456, 0.406], std=[0.229, 0.224, 0.225] |
| Quantization | Mixed precision (INT8 base, sensitive layers in INT16) |
| Target chip | ESP32-P4 (reused on ESP32-S31) |

## Class labels

The label ids below match the 0-based indices produced by PyTorch's `ImageFolder` over the RAF-DB numeric folder names:

| id | Emotion | Train count |
|---|---|---|
| 0 | surprise | 1,290 |
| 1 | fear | 281 |
| 2 | disgust | 717 |
| 3 | happiness | 4,772 |
| 4 | sadness | 1,982 |
| 5 | anger | 705 |
| 6 | neutral | 2,524 |

## Accuracy

Evaluated on RAF-DB with the training-time image augmentation disabled. "Train (FP32)" is measured on a balanced 256-per-class subset of the training set, so it reflects how well the model memorized the training data (it essentially did).

| Class | Train (FP32) | Valid (FP32) | Valid (INT8 mixed) | Valid size |
|---|---|---|---|---|
| happiness | 100.0% | 95.4% | **94.2%** ✅ | 1,185 |
| neutral | 100.0% | 85.4% | **88.8%** ✅ | 652 |
| surprise | 100.0% | 82.1% | 76.0% | 329 |
| sadness | 100.0% | 79.3% | 65.9% ⚠️ | 478 |
| anger | 100.0% | 71.0% | 65.4% ⚠️ | 162 |
| fear | 100.0% | 47.3% | 32.4% ❌ | 74 |
| disgust | 100.0% | 38.1% | 29.4% ❌ | 160 |
| **Overall** | **100.0%** | **83.8%** | **80.2%** | 3,068 |

### Why minority classes (fear / disgust) underperform

- **Severe class imbalance** — `happiness` has ~4,800 training samples vs 281 for `fear`. Weighted sampling helps but cannot add missing diversity.
- **Inherently confusable pairs** — `disgust`↔`anger` and `fear`↔`surprise` share most of their facial action; human annotators disagree on these too.
- **Quantization amplifies it** — the FP32 top-1 margin is already thin on these classes, so INT8 noise flips predictions more easily.

### Mitigation on the device side

- **Score gating.** Treat predictions with `result.score < 0.5` as `uncertain` and skip the update. This trades recall on weak classes for fewer false positives.
- **Temporal smoothing.** Majority-vote the top-1 across the last 5 frames to suppress single-frame flips.
- **Label collapsing.** If the product only needs "positive / neutral / negative", mapping the 7 classes down to 3 pushes overall accuracy to 90%+ on the same weights.


## How to use the example

### ESP-IDF Required

- On ESP32-P4 this example supports ESP-IDF release/v5.5 and later.
- On ESP32-S31 this example requires the ESP-IDF master branch, configured as a preview target with `idf.py --preview set-target esp32s31`.
- Please follow the [ESP-IDF Programming Guide](https://docs.espressif.com/projects/esp-idf/en/latest/esp32p4/get-started/index.html) to set up the development environment. **We highly recommend** you [Build Your First Project](https://docs.espressif.com/projects/esp-idf/en/latest/esp32p4/get-started/index.html#build-your-first-project) to get familiar with ESP-IDF and make sure the environment is set up correctly.

### Hardware Required

This example runs on either of the following boards:

* **ESP32-S31-Korvo** — integrates a DVP camera, an 800 x 480 RGB LCD and a GT1151 touch panel.
* **ESP32-P4-Function-EV-Board** — paired with a MIPI-CSI camera (SC2336) and a 1024 x 600 MIPI-DSI LCD (EK79007).

Attach the camera and LCD to the board, then connect the `USB-UART` port to a PC with a USB-C cable for power and serial output.

### Build and flash

Build the project and flash it to the board, then run monitor tool to view serial output (replace `PORT` with your board's serial port name):

```c
idf.py -p PORT flash monitor
```

To exit the serial monitor, type ``Ctrl-]``.

See the [ESP-IDF Getting Started Guide](https://docs.espressif.com/projects/esp-idf/en/latest/get-started/index.html) for full steps to configure and use ESP-IDF to build projects.

## Performance

The following latency numbers are measured on this example:

- Average emotion classification time: 870 ms

> **Note**: Since the face detection model is currently running at the same time, it is normal for the emotion_cls model to take longer to process.

## Example output


```shell
I (27) boot: ESP-IDF v5.5.4-362-g6aa351c52d5 2nd stage bootloader
I (28) boot: compile time Apr 21 2026 14:01:10
I (28) boot: Multicore bootloader
I (30) boot: chip revision: v3.1
I (32) boot: efuse block revision: v1.2
I (35) boot.esp32p4: SPI Speed      : 80MHz
I (39) boot.esp32p4: SPI Mode       : DIO
I (43) boot.esp32p4: SPI Flash Size : 16MB
I (47) boot: Enabling RNG early entropy source...
I (51) boot: Partition Table:
I (54) boot: ## Label            Usage          Type ST Offset   Length
I (60) boot:  0 nvs              WiFi data        01 02 00009000 00006000
I (67) boot:  1 phy_init         RF data          01 01 0000f000 00001000
I (73) boot:  2 factory          factory app      00 00 00010000 00600000
I (80) boot:  3 storage          Unknown data     01 82 00610000 009f0000
I (87) boot: End of partition table
I (90) esp_image: segment 0: paddr=00010020 vaddr=40220020 size=355f54h (3497812) map
I (559) esp_image: segment 1: paddr=00365f7c vaddr=30100000 size=001e4h (   484) load
I (561) esp_image: segment 2: paddr=00366168 vaddr=301001f0 size=00048h (    72) load
I (564) esp_image: segment 3: paddr=003661b8 vaddr=4ff20000 size=09e60h ( 40544) load
I (577) esp_image: segment 4: paddr=00370020 vaddr=40000020 size=21f92ch (2226476) map
I (872) esp_image: segment 5: paddr=0058f954 vaddr=4ff29e60 size=0b4b0h ( 46256) load
I (881) esp_image: segment 6: paddr=0059ae0c vaddr=4ff35380 size=033a8h ( 13224) load
I (885) esp_image: segment 7: paddr=0059e1bc vaddr=50108080 size=00020h (    32) load
I (890) boot: Loaded app from partition at offset 0x10000
I (891) boot: Disabling RNG early entropy source...
I (908) hex_psram: vendor id    : 0x0d (AP)
I (908) hex_psram: Latency      : 0x01 (Fixed)
I (909) hex_psram: DriveStr.    : 0x00 (25 Ohm)
I (909) hex_psram: dev id       : 0x03 (generation 4)
I (914) hex_psram: density      : 0x07 (256 Mbit)
I (918) hex_psram: good-die     : 0x06 (Pass)
I (922) hex_psram: SRF          : 0x02 (Slow Refresh)
I (927) hex_psram: BurstType    : 0x00 ( Wrap)
I (931) hex_psram: BurstLen     : 0x03 (2048 Byte)
I (936) hex_psram: BitMode      : 0x01 (X16 Mode)
I (940) hex_psram: Readlatency  : 0x06 (18 cycles@Fixed)
I (945) hex_psram: DriveStrength: 0x00 (1/1)
I (949) MSPI Timing: Enter psram timing tuning
I (1108) esp_psram: Found 32MB PSRAM device
I (1108) esp_psram: Speed: 250MHz
I (1109) hex_psram: psram CS IO is dedicated
I (1109) cpu_start: Multicore app
I (1932) esp_psram: SPI SRAM memory test OK
I (1942) cpu_start: GPIO 38 and 37 are used as console UART I/O pins
I (1942) cpu_start: Pro cpu start user code
I (1943) cpu_start: cpu freq: 400000000 Hz
I (1945) app_init: Application information:
I (1948) app_init: Project name:     emotion_recognition
I (1953) app_init: App version:      44d16c392-dirty
I (1958) app_init: Compile time:     Apr 21 2026 14:01:02
I (1963) app_init: ELF file SHA256:  ce03e2be4...
I (1968) app_init: ESP-IDF:          v5.5.4-362-g6aa351c52d5
I (1973) efuse_init: Min chip rev:     v3.1
I (1977) efuse_init: Max chip rev:     v3.99 
I (1981) efuse_init: Chip rev:         v3.1
I (1985) heap_init: Initializing. RAM available for dynamic allocation:
I (1991) heap_init: At 4FF3A6C0 len 00080900 (514 KiB): RETENT_RAM
I (1997) heap_init: At 4FFBAFC0 len 00004BF0 (18 KiB): RAM
I (2003) heap_init: At 501080A0 len 00007F60 (31 KiB): RTCRAM
I (2008) heap_init: At 30100238 len 00001DC8 (7 KiB): SPM
I (2013) esp_psram: Adding pool of 32768K of PSRAM memory to heap allocator
I (2021) spi_flash: detected chip: gd
I (2023) spi_flash: flash io: dio
I (2026) sleep_gpio: Configure to isolate all GPIO pins in sleep state
I (2033) sleep_gpio: Enable automatic switching of GPIO sleep configuration
I (2040) main_task: Started on CPU0
I (2043) esp_psram: Reserving pool of 32K of internal memory for DMA/internal allocations
I (2051) main_task: Calling app_main()
I (2055) ESP32_P4_EV: MIPI DSI PHY Powered on
I (2060) ESP32_P4_EV: Install MIPI DSI LCD control panel
I (2063) ESP32_P4_EV: Install EK79007 LCD control panel
I (2068) ek79007: version: 1.0.4
I (2259) ESP32_P4_EV: Display initialized
I (2259) ESP32_P4_EV: Setting LCD backlight: 100%
I (2259) ESP32_P4_EV: Setting LCD backlight: 100%
I (2263) sc2336: Detected Camera sensor PID=0xcb3a
I (2341) cam: version: 2.0.1
I (2341) cam: driver:  MIPI-CSI
I (2341) cam: card:    MIPI-CSI
I (2341) cam: bus:     esp32p4:MIPI-CSI
I (2342) cam: Width=1024 Height=600
W (2346) ISP_AWB: subwindow size (614 x 360) is not divisible by AWB subwindow blocks grid (5 x 5).                     Resolution will be floored to the nearest divisible value.
E (2361) ISP: gamma xcoord error
E (2364) ISP: gamma xcoord error
E (2367) ISP: gamma xcoord error
E (2413) ISP: gamma xcoord error
E (2413) ISP: gamma xcoord error
E (2413) ISP: gamma xcoord error
I (2414) esp_painter: Painter initialized: 1024x600, format: 0
W (2418) FbsLoader: There is only one model in the flatbuffers, ignore the input model name!
I (2747) main_task: Returned from app_main()
W (2816) FbsLoader: There is only one model in the flatbuffers, ignore the input model name!
W (2979) dl::Model: Minimize() will delete variables not used in model inference, which will make it impossible to test or debug the model.
I (3657) emotion_pipeline: Face[0]: emotion=surprise score=0.357
I (4605) emotion_pipeline: Face[0]: emotion=neutral score=0.886
I (5508) emotion_pipeline: Face[0]: emotion=neutral score=0.944
I (6386) emotion_pipeline: Face[1]: emotion=neutral score=0.399
```
