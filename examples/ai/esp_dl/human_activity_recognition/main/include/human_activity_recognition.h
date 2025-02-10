/*
 * SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once
#include <vector>
#include <map>
#include "dl_model_base.hpp"
#include "dl_module_softmax.hpp"

/**
 * @brief Data preprocessing class, responsible for normalizing the raw feature data and quantizing it into the model input format
 *
 */
class DataPreprocessor {
public:
    /**
     * @brief Construct a new Data Preprocessor object
     *
     * @param model Pointer to the model handle
     * @param mean Pointer to an array containing the mean values for normalization
     * @param std Pointer to an array containing the standard deviation values for normalization
     * @param feature_size The number of features in the input data (561 for the HAR dataset)
     */
    DataPreprocessor(dl::Model* model,
                     const float* mean,
                     const float* std,
                     size_t feature_size);

    /**
     * @brief Perform data preprocessing and quantization
     *
     * @param input_data  Array of raw feature data (length must equal feature_size)
     * @param quant_buffer Output buffer for storing the quantized int8 data (must be pre-allocated)
     */
    void process(const float* input_data, int8_t* quant_buffer);
private:
    dl::Model* m_model;         /*!< Pointer to the model handle */
    std::vector<float> m_mean;  /*!< Mean values for feature normalization */
    std::vector<float> m_std;   /*!< Standard deviation values for normalization */
    float m_input_scale;        /*!< Input quantization scale */
    size_t m_feature_size;      /*!< Number of feature dimensions (fixed to 561) */
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
     * @brief Destructor for the Data Postprocessor class
     *
     */
    ~DataPostprocessor();

    /**
     * @brief Processes the model output, applying softmax and extracting predictions
     *
     * @return int The predicted class label
     */
    int process();
private:
    dl::Model* m_model;                     /*!< Pointer to the model handle */
    dl::module::Softmax *m_softmax_module;  /*!< Softmax module for probability calculation */
    dl::TensorBase *m_model_output;         /*!< Raw model output tensor */
    dl::TensorBase *m_output;               /*!< Processed output tensor */
    float m_output_scale;                   /*!< Output dequantization scale */
};

class HumanActivityRecognition {
public:
    /**
     * @brief Construct a new Human Activity Recognition object
     *
     * @param model_name Model name
     * @param mean Pointer to an array containing the mean values for normalization
     * @param std Pointer to an array containing the standard deviation values for normalization
     * @param feature_size Number of feature dimensions (561 for the HAR dataset)
     */
    HumanActivityRecognition(const char* model_name,
                             const float* mean,
                             const float* std,
                             size_t feature_size);

    /**
     * @brief Destroy the Human Activity Recognition object
     *
     */
    ~HumanActivityRecognition();

    /**
     * @brief Runs inference on the provided input features
     *
     * @param input_features Pointer to the input feature array
     * @return int The predicted class label
     */
    int predict(const float* input_features);
private:
    dl::Model* m_model = nullptr;                  /*!< Pointer to the model handle */
    DataPreprocessor* m_preprocessor = nullptr;    /*!< Preprocessing component */
    DataPostprocessor *m_postprocessor = nullptr;  /*!< Postprocessing component */
    int8_t* m_quant_buffer = nullptr;              /*!< Buffer for quantized data (length equals feature_size) */
    size_t m_feature_size;                         /*!< Number of feature dimensions */
};
