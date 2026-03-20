/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

#include "dl_image_preprocessor.hpp"
#include "dl_tensor_base.hpp"
#include "imagenet_cls.hpp"
#include <map>

/**
 * @brief Prediction result returned by SelfLearningClassifier::predict()
 */
struct PredictResult {
    int class_id;       ///< Predicted class ID, -1 means unknown
    float distance;     ///< Distance to nearest prototype
    float confidence;   ///< Confidence score (0~100%)
};

/**
 * @brief Few-shot self-learning classifier based on prototype network.
 *
 * Uses a pretrained MobileNetV2 to extract features, and performs
 * nearest-neighbor matching against stored prototypes for classification.
 *
 * Usage:
 *   1. Create an instance with the model name
 *   2. Call enroll(img, class_id) to add training samples
 *   3. Call predict(img) to classify new images
 */
class SelfLearningClassifier {
public:
    /**
     * @brief Constructor
     * @param model_name      espdl model name
     * @param max_prototypes  max prototypes per class (default 7)
     */
    SelfLearningClassifier(const char *model_name, int max_prototypes = 7);
    ~SelfLearningClassifier();

    /**
     * @brief Enroll an image into the specified class
     * @param img       Input image (RGB888)
     * @param class_id  Class ID
     */
    void enroll(const dl::image::img_t &img, int class_id);

    /**
     * @brief Predict the class of an input image
     * @param img Input image (RGB888)
     * @return Prediction result (class_id = -1 if unknown)
     */
    PredictResult predict(const dl::image::img_t &img);

    int get_class_count() const;
    int get_feat_dim() const;

private:
    struct CategoryData {
        float **prototypes;
        int count;
        int total_samples;
        float avg_intra_dist;
    };

    imagenet_cls::MobileNetV2FeatureExtractor *m_extractor;
    int m_feat_dim;
    int m_max_prototypes;
    std::map<int, CategoryData *> m_categories;

    CategoryData *new_category();
    void free_category(CategoryData *cat);
    float *copy_feat(const float *src);
    void add_prototype(CategoryData *cat, const float *feat);

    static void normalize(float *feat, int dim);
    static float euclidean_distance(const float *a, const float *b, int dim);
    static float similarity_score(float distance);
};
