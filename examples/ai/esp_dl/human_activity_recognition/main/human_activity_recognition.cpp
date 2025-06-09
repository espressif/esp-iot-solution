/*
 * SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "esp_heap_caps.h"
#include "human_activity_recognition.h"

DataPreprocessor::DataPreprocessor(dl::Model *model,
                                   const float *mean,
                                   const float *std,
                                   size_t feature_size)
    : m_model(model),
      m_mean(mean, mean + feature_size),
      m_std(std, std + feature_size),
      m_feature_size(feature_size)
{
    auto inputs = m_model->get_inputs();
    if (!inputs.empty()) {
        dl::TensorBase* input_tensor = inputs.begin()->second;
        m_input_scale = 1.0f / DL_SCALE(input_tensor->exponent);
    }
}

void DataPreprocessor::process(const float *input_data, int8_t *quant_buffer)
{
    for (size_t i = 0; i < m_feature_size; ++i) {
        float normalized = (input_data[i] - m_mean[i]) / m_std[i];
        quant_buffer[i] = dl::quantize<int8_t>(normalized, m_input_scale);
    }
}

DataPostprocessor::DataPostprocessor(dl::Model* model) : m_model(model)
{
    auto outputs = m_model->get_outputs();
    m_softmax_module = new dl::module::Softmax(nullptr, -1, dl::MODULE_NON_INPLACE, dl::QUANT_TYPE_SYMM_8BIT);
    if (!outputs.empty()) {
        m_model_output = outputs.begin()->second;
        m_output_scale = DL_SCALE(m_model_output->exponent);
    }
    m_output = new dl::TensorBase(m_model_output->shape, nullptr, 0, dl::DATA_TYPE_FLOAT);
}

DataPostprocessor::~DataPostprocessor()
{
    if (m_output) {
        delete m_output;
        m_output = nullptr;
    }

    if (m_softmax_module) {
        delete m_softmax_module;
        m_softmax_module = nullptr;
    }
}

int DataPostprocessor::process()
{
    auto outputs = m_model->get_outputs();
    if (outputs.empty()) {
        return -1;
    }

    m_model_output = outputs.begin()->second;
    m_softmax_module->run(m_model_output, m_output);

    float *output_ptr = (float *)m_output->data;
    int size = m_output->get_size();

    auto max_iter = std::max_element(output_ptr, output_ptr + size);
    int max_index = std::distance(output_ptr, max_iter);
    return max_index;
}

HumanActivityRecognition::HumanActivityRecognition(const char *model_name,
                                                   const float *mean,
                                                   const float *std,
                                                   size_t feature_size)
{
    m_model = new dl::Model(model_name, fbs::MODEL_LOCATION_IN_FLASH_PARTITION);
    m_model->minimize();
    m_preprocessor = new DataPreprocessor(m_model, mean, std, feature_size);
    m_postprocessor = new DataPostprocessor(m_model);
    m_feature_size = feature_size;
    m_quant_buffer = (int8_t*)heap_caps_calloc(feature_size * sizeof(int8_t), 1, MALLOC_CAP_SPIRAM);
}

HumanActivityRecognition::~HumanActivityRecognition()
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

int HumanActivityRecognition::predict(const float *input_features)
{
    m_preprocessor->process(input_features, m_quant_buffer);

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
