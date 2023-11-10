[![Component Registry](https://components.espressif.com/components/espressif/ir_learn/badge.svg)](https://components.espressif.com/components/espressif/ir_learn)

| Supported Targets | ESP32 | ESP32-C3 | ESP32-C6 | ESP32-H2 | ESP32-S2 | ESP32-S3 |
| ----------------- | ----- | -------- | -------- | -------- | -------- | -------- |
## Component IR Learn

This is an RMT-based `IR Learn` component that can receive to analyze infrared signals with a `38KHz` carrier. The signal collected by the component is saved and forwarded in the form of `raw`, and the specific analysis of the infrared protocol is not supported now.

## How to Use Example

### Hardware Required

* A development board with supported SoC mentioned in the above `Supported Targets` table
* An USB cable for power supply and programming
* A 5mm infrared LED (e.g. IR333C) used to transmit encoded IR signals
* An infrared receiver module (e.g. IRM-3638T), which integrates a demodulator and AGC circuit
### Hardware Connection

```
   IR Receiver (IRM-3638T)                 ESP Board                        IR Transmitter (IR333C)
+--------------------------+       +----------------------+              +---------------------------+
|                        RX+-------+IR_RX_GPIO  IR_TX_GPIO+--------------+TX                         |
|                          |       |                      |              |                           |
|                       3V3+-------+3V3                 5V+--------------+VCC                        |
|                          |       |                      |              |                           |
|                       GND+-------+GND                GND+--------------+GND                        |
+--------------------------+       +----------------------+              +---------------------------+
```

