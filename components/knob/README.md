## Component Knob

`Knob` is the component that provides the software PCNT, it can be used on chips(esp32c2, esp32c3) that do not have PCNT hardware capabilities. By using this component, you can quickly use a physical encoder, such as the EC11 encoder.

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