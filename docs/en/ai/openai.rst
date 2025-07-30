OpenAI
=============
:link_to_translation:`en:[English]`

The `openai` component is a porting version of  `OpenAI-ESP32 <https://github.com/me-no-dev/OpenAI-ESP32>`_ arduino library, which simplifies the process of invoking the OpenAI API on `ESP-IDF`. 

This library supports the following OpenAI API capabilities:

- [ ] `Responses <https://platform.openai.com/docs/api-reference/responses>`_ [1]_
- [x] `Chat Completions <https://platform.openai.com/docs/api-reference/chat>`_: Supports text, image, and audio input; outputs text. [2]_ Streaming output is not currently supported.
- [x] `Audio <https://platform.openai.com/docs/api-reference/audio>`_: Speech & Transcription. Speech supports streaming output.
- [x] `Images <https://platform.openai.com/docs/api-reference/images>`_: Create image, Edits & Variations
- [x] `Embeddings <https://platform.openai.com/docs/api-reference/embeddings>`_
- [ ] `Evals <https://platform.openai.com/docs/api-reference/evals>`_
- [ ] `Fine-tuning <https://platform.openai.com/docs/api-reference/fine-tuning>`_
- [ ] `Batch <https://platform.openai.com/docs/api-reference/batch>`_
- [ ] `Files <https://platform.openai.com/docs/api-reference/files>`_
- [x] `Moderations <https://platform.openai.com/docs/api-reference/moderations>`_

.. note::
   Real-time capabilities can be implemented using the following libraries:
   
   - **WebSocket solution**:
     - https://github.com/openai/openai-realtime-embedded
   - **WebRTC solution**:
     - https://github.com/espressif/esp-webrtc-solution/tree/main/solutions/openai_demo

.. [1] Currently, please use the Chat Completions API instead.
.. [2] Multimodal input requires using ``multiModalMessage``.

For more details, please refer to `OpenAI <https://platform.openai.com/docs/api-reference>`_  API REFERENCE.

FAQ
-----

Error `Failed to allocate request buffer!` when using `audioTranscription->file`
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

This error indicates that dynamic memory allocation failed, usually due to insufficient available memory. The required memory size depends on the current available RAM or PSRAM size.

**Solutions**

1. **Enable PSRAM**: If the device supports PSRAM, enable PSRAM to increase available memory and enable the `CONFIG_SPIRAM_USE_MALLOC` macro to use PSRAM.

2. **Reduce the audio data size**: For example, reduce the audio sampling rate or the length of the audio.


API Reference
-------------

.. include-build-file:: inc/OpenAI.inc