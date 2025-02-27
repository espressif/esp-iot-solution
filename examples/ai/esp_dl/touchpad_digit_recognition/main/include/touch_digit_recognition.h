/*
 * SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once
#include <vector>
#include <map>
#include "dl_model_base.hpp"

/**
 * @brief Data preprocessing class, responsible for processing the raw feature data and quantizing it into the model input format
 *
 */
class DataPreprocessor {
public:
    /**
     * @brief Construct a new Data Preprocessor object
     *
     * @param model Pointer to the model handle
     * @param feature_size The number of features in the input data
     */
    DataPreprocessor(dl::Model* model, size_t feature_size);

    /**
     * @brief Perform data preprocessing and quantization
     *
     * @param input_data Array of raw feature data (length must equal feature_size)
     * @param quant_buffer Output buffer for storing the quantized int8 data (must be pre-allocated)
     */
    void process(const uint8_t* input_data, int8_t* quant_buffer);
private:
    dl::Model* m_model;     /*!< Pointer to the model handle */
    float m_input_scale;    /*!< Input quantization scale */
    size_t m_feature_size;  /*!< Number of feature dimensions */
};

/**
 * @brief Data postprocessing class, responsible for handling the model output
 *
 */
class DataPostprocessor {
public:
    /**
     * @brief Construct a new Data Postprocessor object
     *
     * @param model Pointer to the model handle
     */
    explicit DataPostprocessor(dl::Model* model);

    /**
     * @brief Destroy the Data Postprocessor object
     *
     */
    ~DataPostprocessor();

    /**
     * @brief Process model output and extract final results
     *
     * @return int The predicted class label
     */
    int process();
private:
    dl::Model* m_model;              /*!< Pointer to the model handle */
    dl::TensorBase* m_model_output;  /*!< Raw model output tensor */
};

/**
 * @brief Touch Digit Recognition class, responsible for managing the entire recognition process
 *
 */
class TouchDigitRecognition {
public:
    /**
     * @brief Construct a new Touch Digit Recognition object
     *
     * @param model_name Model name
     * @param feature_size Number of feature dimensions
     */
    TouchDigitRecognition(const char* model_name, size_t feature_size);

    /**
     * @brief Destroy the Touch Digit Recognition object
     *
     */
    ~TouchDigitRecognition();

    /**
     * @brief Runs inference on the provided input features
     *
     * @param input_data Pointer to the input feature array
     * @return int The predicted class label
     */
    int predict(const uint8_t* input_data);
private:
    dl::Model* m_model = nullptr;                  /*!< Pointer to the model handle */
    DataPreprocessor *m_preprocessor = nullptr;    /*!< Preprocessing component */
    DataPostprocessor *m_postprocessor = nullptr;  /*!< Postprocessing component */
    int8_t* m_quant_buffer = nullptr;              /*!< Buffer for quantized data (length equals feature_size) */
    size_t m_feature_size;                         /*!< Number of feature dimensions */
};
