# Component: Button
[documentation](https://docs.espressif.com/projects/espressif-esp-iot-solution/en/latest/audio/pwm_audio.html)

The PWM audio function uses the internal LEDC peripheral in ESP32 to generate PWM audio, which does not need an external audio Codec chip.

## Features

- Allows any GIPO with output capability as an audio output pin
- Supports 8-bit ~ 10-bit PWM resolution
- Supports stereo
- Supports 8 ～ 48 KHz sampling rate

[Here](https://github.com/espressif/esp-iot-solution/tree/master/examples/audio/wav_player) is a simple player example that uses PWM to output audio.
