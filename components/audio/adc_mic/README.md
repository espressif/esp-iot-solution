[![Component Registry](https://components.espressif.com/components/espressif/adc_mic/badge.svg)](https://components.espressif.com/components/espressif/adc_mic)

# Component: ADC Mic

The **ADC MIC** can collect analog microphone data through the ADC.

Currently supported features:
* Supports single-channel/multi-channel ADC audio sampling.
* Compliant with the esp_codec_dev architecture.

## Add component to your project

Please use the component manager command `add-dependency` to add the `adc_mic` to your project's dependency, during the `CMake` step the component will be downloaded automatically

```
idf.py add-dependency "espressif/adc_mic=*"
```

## Known Issues

* In IDF versions lower than ``5.2``, the internal ring buffer does not automatically clear old data when full. Please ensure old data is cleared in time or manually refresh the buffer.
