[![Component Registry](https://components.espressif.com/components/espressif/openai/badge.svg)](https://components.espressif.com/components/espressif/openai)

## Open AI component

The `openai` component is a porting version of [OpenAI-ESP32](https://github.com/me-no-dev/OpenAI-ESP32) arduino library, which simplifies the process of invoking the OpenAI API on `ESP-IDF`.

This library supports the following OpenAI API capabilities:
- [ ] [Responses](https://platform.openai.com/docs/api-reference/responses) [^1]
- [x] [Chat Completions](https://platform.openai.com/docs/api-reference/chat): Supports text, image, and audio input; outputs text. [^2] Streaming output is not currently supported.
- [x] [Audio](https://platform.openai.com/docs/api-reference/audio): Speech & Transcription. Speech supports streaming output.
- [x] [Images](https://platform.openai.com/docs/api-reference/images): Create image, Edits & Variations
- [x] [Embeddings](https://platform.openai.com/docs/api-reference/embeddings)
- [ ] [Evals](https://platform.openai.com/docs/api-reference/evals)
- [ ] [Fine-tuning](https://platform.openai.com/docs/api-reference/fine-tuning)
- [ ] [Batch](https://platform.openai.com/docs/api-reference/batch)
- [ ] [Files](https://platform.openai.com/docs/api-reference/files)
- [x] [Moderations](https://platform.openai.com/docs/api-reference/moderations)

Note: Real-time capabilities can be implemented using the following libraries:
- WebSocket solution:
  - https://github.com/openai/openai-realtime-embedded
- WebRTC solution:
  - https://github.com/espressif/esp-webrtc-solution/tree/main/solutions/openai_demo

[^1]: Currently, please use the Chat Completions API instead.
[^2]: Multimodal input requires using `multiModalMessage`.

For more details, please refer to [OpenAI](https://platform.openai.com/docs/api-reference) API REFERENCE.

### User Guide

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