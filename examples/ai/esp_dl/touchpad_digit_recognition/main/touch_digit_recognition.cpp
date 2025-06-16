/*
 * SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include "esp_heap_caps.h"
#include "esp_log.h"
#include "touch_digit_recognition.h"

static const char* TAG = "TouchDigitRecognition";

DataPreprocessor::DataPreprocessor(dl::Model* model, size_t feature_size)
    : m_model(model),
      m_feature_size(feature_size)
{
    auto inputs = m_model->get_inputs();
    if (!inputs.empty()) {
        dl::TensorBase* input_tensor = inputs.begin()->second;
        m_input_scale = 1.0f / DL_SCALE(input_tensor->exponent);
    }
}

void DataPreprocessor::process(const uint8_t* input_data, int8_t* quant_buffer)
{
    for (size_t i = 0; i < m_feature_size; i++) {
        int8_t value = (input_data[i] == 0 ? -1 : 1);
        quant_buffer[i] = dl::quantize<int8_t>((float)value, m_input_scale);
    }
}

DataPostprocessor::DataPostprocessor(dl::Model* model)
    : m_model(model)
{
}

DataPostprocessor::~DataPostprocessor()
{
    if (m_model_output) {
        delete m_model_output;
        m_model_output = nullptr;
    }
}

int DataPostprocessor::process()
{
    auto outputs = m_model->get_outputs();
    if (outputs.empty()) {
        return -1;
    }

    m_model_output = outputs.begin()->second;
    float *output_ptr = (float *)m_model_output->data;
    int size = m_model_output->get_size();
    auto max_iter = std::max_element(output_ptr, output_ptr + size);
    int max_index = std::distance(output_ptr, max_iter);
    ESP_LOGI(TAG, "Predict result: %d", max_index);
    return max_index;
}

TouchDigitRecognition::TouchDigitRecognition(const char* model_name, size_t feature_size)
{
    m_model = new dl::Model(model_name, fbs::MODEL_LOCATION_IN_FLASH_PARTITION);
    m_model->minimize();
    m_preprocessor = new DataPreprocessor(m_model, feature_size);
    m_postprocessor = new DataPostprocessor(m_model);
    m_feature_size = feature_size;
    m_quant_buffer = (int8_t*)heap_caps_calloc(feature_size * sizeof(int8_t), 1, MALLOC_CAP_SPIRAM);
}

TouchDigitRecognition::~TouchDigitRecognition()
{
    if (m_model) {
        delete m_model;
        m_model = nullptr;
    }

    if (m_preprocessor) {
        delete m_preprocessor;
        m_preprocessor = nullptr;
    }

    if (m_postprocessor) {
        delete m_postprocessor;
        m_postprocessor = nullptr;
    }

    if (m_quant_buffer) {
        free(m_quant_buffer);
        m_quant_buffer = nullptr;
    }
}

int TouchDigitRecognition::predict(const uint8_t* input_data)
{
    m_preprocessor->process(input_data, m_quant_buffer);

    std::map<std::string, dl::TensorBase*> inputs;
    auto model_inputs = m_model->get_inputs();

    for (auto &input_pair : model_inputs) {
        dl::TensorBase* tensor = new dl::TensorBase(
        {1, static_cast<int>(m_feature_size)},
        m_quant_buffer,
        input_pair.second->exponent,
        input_pair.second->dtype,
        false,
        MALLOC_CAP_SPIRAM
        );
        inputs.emplace(input_pair.first, tensor);
    }

    m_model->run(inputs);

    for (auto &input : inputs) {
        delete input.second;
    }

    return m_postprocessor->process();
}
