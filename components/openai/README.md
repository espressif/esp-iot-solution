[![Component Registry](https://components.espressif.com/components/espressif/openai/badge.svg)](https://components.espressif.com/components/espressif/openai)

## Open AI component

The `openai` component is a porting version of [OpenAI-ESP32](https://github.com/me-no-dev/OpenAI-ESP32) arduino library, which simplifies the process of invoking the OpenAI API on `ESP-IDF`. This library provides extensive support for the majority of OpenAI API functionalities, except for `files` and `fine-tunes`. For more details, please refer to [OpenAI](https://platform.openai.com/docs/api-reference) API REFERENCE.

### Open AI User Guide

Please refer: https://docs.espressif.com/projects/esp-iot-solution/en/latest/ai/openai.html

### Add component to your project

Please use the component manager command `add-dependency` to add the `openai` to your project's dependency, during the `CMake` step the component will be downloaded automatically

```
idf.py add-dependency "espressif/openai=*"
```

### How To Use

1. You need to register an OpenAI key.
2. Pass the key into the initialization function of the component:
    ```
    OpenAI_t *openai = OpenAICreate(openai_key);
    ```
3. Connect to a WiFi network that has access to the OpenAI servers.