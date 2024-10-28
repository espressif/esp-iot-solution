[![Component Registry](https://components.espressif.com/components/espressif/knob/badge.svg)](https://components.espressif.com/components/espressif/knob)

## Component Knob

`Knob` is the component that provides the software quadrature decoding, it can be used on chips(esp32c2, esp32c3) that do not have PCNT hardware capabilities. By using this component, you can quickly use a physical encoder, such as the EC11 encoder.

Features:

1. Support multiple knobs
2. Support each event can register its own callback
3. Support setting the upper and lower count limits

List of supported events:

 * Knob left
 * Knob right
 * Knob high limit
 * Knob low limit
 * Knob back to zero

### Examples

[USB Surface Dial](https://github.com/espressif/esp-iot-solution/tree/master/examples/usb/device/usb_surface_dial)


`Note`: This component is only suitable for decoding low-speed rotary encoders such as EC11, and does not guarantee the complete correctness of the pulse count. For high-speed and accurate calculations, please use hardware [PCNT](https://docs.espressif.com/projects/esp-idf/zh_CN/latest/esp32/api-reference/peripherals/pcnt.html?highlight=pcnt)

* | Hardware PCNT Supported Targets | ESP32 | ESP32-C6 | ESP32-H2 | ESP32-S2 | ESP32-S3 |
  | ------------------------------- | ----- | -------- | -------- | -------- | -------- |