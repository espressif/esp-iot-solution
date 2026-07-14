/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include "predict.h"

#include <math.h>
#include <stdint.h>
#include <string.h>

#include "tensorflow/lite/micro/micro_interpreter.h"
#include "tensorflow/lite/micro/micro_mutable_op_resolver.h"
#include "tensorflow/lite/micro/system_setup.h"
#include "tensorflow/lite/schema/schema_generated.h"

#include "model.h"

namespace {

constexpr size_t kInputChannels = 3;
constexpr size_t kInputLength = 150;
constexpr size_t kInputCount = 450;
constexpr size_t kOutputCount = 5;
constexpr size_t kTensorArenaSize = 65536;

alignas(16) uint8_t g_tensor_arena[kTensorArenaSize];
tflite::MicroInterpreter* g_interpreter = nullptr;
TfLiteTensor* g_input_tensor = nullptr;
TfLiteTensor* g_output_tensor = nullptr;
bool g_initialized = false;

const float g_normalize_mean[kInputChannels] = {
    -3.78983498f, -3.15663528f, -0.36433959f
};

const float g_normalize_std[kInputChannels] = {
    39.6913605f, 60.1708412f, 77.9114609f
};

inline float ApplyPreprocess(float value, size_t channel)
{
    value *= 1.0f;
    return (value - g_normalize_mean[channel]) / g_normalize_std[channel];
}

inline int8_t QuantizeInt8(float value, float scale, int zero_point)
{
    if (scale == 0.0f) {
        return static_cast<int8_t>(zero_point);
    }
    const float transformed = value / scale + static_cast<float>(zero_point);
    int quantized = static_cast<int>(roundf(transformed));
    if (quantized < -128) {
        quantized = -128;
    }
    if (quantized > 127) {
        quantized = 127;
    }
    return static_cast<int8_t>(quantized);
}

inline size_t SourceFlatIndex(size_t timestep, size_t channel)
{
    return timestep * kInputChannels + channel;
}

}  // namespace

bool gesture_inference_micro_simple_norm_model_init()
{
    if (g_initialized) {
        return true;
    }

    tflite::InitializeTarget();
    const tflite::Model* model = tflite::GetModel(gesture_inference_micro_simple_norm_model_tflite);
    if (model->version() != TFLITE_SCHEMA_VERSION) {
        MicroPrintf("Model schema mismatch: %d != %d", model->version(), TFLITE_SCHEMA_VERSION);
        return false;
    }

    static tflite::MicroMutableOpResolver<7> resolver;
    if (resolver.AddPad() != kTfLiteOk) {
        MicroPrintf("Failed to add PAD operator.");
        return false;
    }

    if (resolver.AddReshape() != kTfLiteOk) {
        MicroPrintf("Failed to add RESHAPE operator.");
        return false;
    }

    if (resolver.AddConv2D() != kTfLiteOk) {
        MicroPrintf("Failed to add CONV_2D operator.");
        return false;
    }

    if (resolver.AddMaxPool2D() != kTfLiteOk) {
        MicroPrintf("Failed to add MAX_POOL_2D operator.");
        return false;
    }

    if (resolver.AddAveragePool2D() != kTfLiteOk) {
        MicroPrintf("Failed to add AVERAGE_POOL_2D operator.");
        return false;
    }

    if (resolver.AddFullyConnected() != kTfLiteOk) {
        MicroPrintf("Failed to add FULLY_CONNECTED operator.");
        return false;
    }

    if (resolver.AddSoftmax() != kTfLiteOk) {
        MicroPrintf("Failed to add SOFTMAX operator.");
        return false;
    }

    static tflite::MicroInterpreter static_interpreter(
        model,
        resolver,
        g_tensor_arena,
        kTensorArenaSize);
    g_interpreter = &static_interpreter;

    if (g_interpreter->AllocateTensors() != kTfLiteOk) {
        MicroPrintf("AllocateTensors() failed");
        return false;
    }

    g_input_tensor = g_interpreter->input(0);
    g_output_tensor = g_interpreter->output(0);
    g_initialized = true;
    return true;
}

bool gesture_inference_micro_simple_norm_model_predict(
    const float* input,
    size_t input_len,
    float* output,
    size_t output_len)
{
    if (!g_initialized && !gesture_inference_micro_simple_norm_model_init()) {
        return false;
    }
    if (input == nullptr || output == nullptr) {
        return false;
    }
    if (input_len != kInputCount || output_len != kOutputCount) {
        return false;
    }

    if (true) {
        int8_t* input_ptr = g_input_tensor->data.int8;
        size_t dst = 0;
        for (size_t t = 0; t < kInputLength; ++t) {
            for (size_t c = 0; c < kInputChannels; ++c) {
                const float normalized = ApplyPreprocess(input[SourceFlatIndex(t, c)], c);
                input_ptr[dst++] = QuantizeInt8(
                                       normalized,
                                       0.0834271461f,
                                       7);
            }
        }
    } else {
        float* input_ptr = g_input_tensor->data.f;
        size_t dst = 0;
        for (size_t t = 0; t < kInputLength; ++t) {
            for (size_t c = 0; c < kInputChannels; ++c) {
                input_ptr[dst++] = ApplyPreprocess(input[SourceFlatIndex(t, c)], c);
            }
        }
    }

    if (g_interpreter->Invoke() != kTfLiteOk) {
        MicroPrintf("Invoke failed");
        return false;
    }

    if (true) {
        const int8_t* output_ptr = g_output_tensor->data.int8;
        constexpr int kOutputZeroPoint = -128;
        constexpr float kOutputScale = 0.00390625f;
        for (size_t i = 0; i < output_len; ++i) {
            output[i] = (static_cast<int>(output_ptr[i]) - kOutputZeroPoint) * kOutputScale;
        }
    } else {
        memcpy(output, g_output_tensor->data.f, output_len * sizeof(float));
    }
    return true;
}

int gesture_inference_micro_simple_norm_model_predict_label(const float* input, size_t input_len)
{
    float output[5] = {0};
    if (!gesture_inference_micro_simple_norm_model_predict(input, input_len, output, 5)) {
        return -1;
    }
    int best_index = 0;
    for (size_t i = 1; i < 5; ++i) {
        if (output[i] > output[best_index]) {
            best_index = static_cast<int>(i);
        }
    }
    return best_index;
}

// Project: gesture_inference_micro_simple_norm
// Export backend: tflite-micro
// Exported reference sample index: 56
// model_predict() expects raw float input flattened in [L, C] order.
// Training/config semantics remain [C, L]; export writes test_input in deployed layout.
// Reference label for this sample: 1
