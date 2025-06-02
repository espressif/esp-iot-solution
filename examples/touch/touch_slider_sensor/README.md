# Touch Slider Example

**Note:** This example is for developers testing only. It is not intended for production use.

This example demonstrates how to use the touch_slider_sensor component to implement touch button functionality on ESP32 devices.

## How to Use Example

### Hardware

This example uses the ESP32-S2S3-Touch-DevKit-1 MainBoard v1.1 and the touch sub-board ESP32-S2S3-Touch-DevKit-1 Slider Board V1.0. For more details, please refer to the [reference documentation](https://docs.espressif.com/projects/esp-dev-kits/en/latest/esp32s2/esp32-s2-touch-devkit-1/user_guide.html). Refer to [Touch Sensor Application Note](https://github.com/espressif/esp-iot-solution/blob/release/v1.0/documents/touch_pad_solution/touch_sensor_design_en.md) if you are designing your own hardware.

### Build and Flash

Build the project and flash it to the board:

```bash
idf.py build
idf.py -p PORT flash
```

Replace PORT with your serial port name.

### Example Output

The example will output touch sensor readings to the serial monitor:

```
I (284) main_task: Calling app_main()
I (284) touch_lowlevel: Touch sensor lowlevel (v0.8.1) configured with 6 channels
I (284) touch_fsm: Touch Sensor FSM created, version: 0.7.0
I (294) touch_fsm: Control command 0
I (294) touch_lowlevel: Touch sensor lowlevel started
Slide 0
I (934) touch_fsm: Channel 2: Baseline found: 34321
I (934) touch_fsm: Channel 4: Baseline found: 34136
I (934) touch_fsm: Channel 6: Baseline found: 33196
I (934) touch_fsm: Channel 12: Baseline found: 31270
I (934) touch_fsm: Channel 10: Baseline found: 31045
I (944) touch_fsm: Channel 8: Baseline found: 28838
pos,9482
pos,9487
pos,9498
pos,9513
pos,9529
pos,9546
pos,9562
pos,9572
pos,9575
pos,9551
pos,9502
pos,9425
pos,9313
pos,9169
pos,8988
pos,8769
pos,8491
pos,8161
Left swipe (speed)
pos,7795
pos,7373
pos,6911
pos,6429
pos,5935
pos,5408
pos,4876
pos,4347
pos,3856
Slide -5626
Left swipe (displacement)
pos,572
pos,580
pos,595
pos,624
pos,675
pos,753
pos,865
pos,1005
pos,1215
pos,1474
pos,1781
Right swipe (speed)
pos,2121
pos,2532
pos,2987
pos,3454
pos,3908
Slide 3336
Right swipe (displacement)
...
```
