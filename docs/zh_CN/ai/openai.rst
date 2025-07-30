OpenAI
=============
:link_to_translation:`en:[English]`

`openai` 组件是 `OpenAI-ESP32 <https://github.com/me-no-dev/OpenAI-ESP32>`_ Arduino 库的移植版本，它简化了在 `ESP-IDF` 上调用 OpenAI API 的过程。

该库支持以下 OpenAI API 能力：

- [ ] `Responses <https://platform.openai.com/docs/api-reference/responses>`_ [1]_
- [x] `Chat Completions <https://platform.openai.com/docs/api-reference/chat>`_: 支持 text, image 与 audio 输入，输出支持 text。[2]_ 暂不支持流式输出。
- [x] `Audio <https://platform.openai.com/docs/api-reference/audio>`_: Speech & Transcription，其中 Speech 支持流式输出。
- [x] `Images <https://platform.openai.com/docs/api-reference/images>`_: Create image & Edits & Variations
- [x] `Embeddings <https://platform.openai.com/docs/api-reference/embeddings>`_
- [ ] `Evals <https://platform.openai.com/docs/api-reference/evals>`_
- [ ] `Fine-tuning <https://platform.openai.com/docs/api-reference/fine-tuning>`_
- [ ] `Batch <https://platform.openai.com/docs/api-reference/batch>`_
- [ ] `Files <https://platform.openai.com/docs/api-reference/files>`_
- [x] `Moderations <https://platform.openai.com/docs/api-reference/moderations>`_

.. note::
   Realtime 能力可以使用以下库实现：
   
   - **WebSocket 方案**:
     - https://github.com/openai/openai-realtime-embedded
   - **WebRTC 方案**:
     - https://github.com/espressif/esp-webrtc-solution/tree/main/solutions/openai_demo

.. [1] 目前，请使用 Chat Completions API 代替。
.. [2] 多模态输入需要使用 ``multiModalMessage``。

更多详情，请参考 `OpenAI <https://platform.openai.com/docs/api-reference>`_ 的 API 参考文档。

FAQ
------

使用 `audioTranscription->file` 报错 `Failed to allocate request buffer!`
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

该错误表明动态内存分配失败，通常是由于可用内存不足导致的。所需的内存大小取决于当前剩余的 RAM 或 PSRAM 大小。

**解决方法**

1. **开启 PSRAM**：如果设备支持 PSRAM，可以开启 PSRAM ，以增加可用内存，并打开宏 `CONFIG_SPIRAM_USE_MALLOC` 以使用 PSRAM。
2. **减少音频的数据量**:减少音频的数据量，例如减少音频的采样率、音频的长度等。

API 参考
------------

.. include-build-file:: inc/OpenAI.inc