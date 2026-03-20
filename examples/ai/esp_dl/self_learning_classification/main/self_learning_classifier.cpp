/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include "self_learning_classifier.hpp"
#include "dl_math.hpp"
#include "esp_log.h"
#include <cstring>
#include <cfloat>

static const char *TAG = "self_learning";

SelfLearningClassifier::SelfLearningClassifier(const char *model_name, int max_prototypes)
    : m_max_prototypes(max_prototypes)
{
    m_extractor = new imagenet_cls::MobileNetV2FeatureExtractor(model_name);
    m_feat_dim = m_extractor->get_feat_dim();
    ESP_LOGI(TAG, "SelfLearningClassifier created, feature dim: %d, max prototypes/class: %d",
             m_feat_dim, m_max_prototypes);
}

SelfLearningClassifier::~SelfLearningClassifier()
{
    for (auto &pair : m_categories) {
        free_category(pair.second);
    }
    m_categories.clear();
    delete m_extractor;
}

void SelfLearningClassifier::enroll(const dl::image::img_t &img, int class_id)
{
    dl::TensorBase *features = m_extractor->run(img);
    float *feat = (float *)features->data;
    normalize(feat, m_feat_dim);

    if (m_categories.find(class_id) == m_categories.end()) {
        m_categories[class_id] = new_category();
        ESP_LOGI(TAG, "Created new class %d", class_id);
    }

    CategoryData *cat = m_categories[class_id];
    add_prototype(cat, feat);

    ESP_LOGI(TAG, "Enrolled to class %d: %d/%d prototypes, %d total samples, avg_intra_dist=%.4f",
             class_id, cat->count, m_max_prototypes, cat->total_samples, cat->avg_intra_dist);
}

PredictResult SelfLearningClassifier::predict(const dl::image::img_t &img)
{
    PredictResult result = {.class_id = -1, .distance = FLT_MAX, .confidence = 0.0f};

    if (m_categories.empty()) {
        ESP_LOGW(TAG, "No enrolled classes, cannot predict");
        return result;
    }

    dl::TensorBase *features = m_extractor->run(img);
    float *feat = (float *)features->data;
    normalize(feat, m_feat_dim);

    float second_min_dist = FLT_MAX;
    int best_class = -1;
    CategoryData *best_cat = nullptr;

    for (auto &pair : m_categories) {
        int cid = pair.first;
        CategoryData *cat = pair.second;
        for (int i = 0; i < cat->count; i++) {
            float dist = euclidean_distance(feat, cat->prototypes[i], m_feat_dim);
            if (dist < result.distance) {
                second_min_dist = result.distance;
                result.distance = dist;
                best_class = cid;
                best_cat = cat;
            } else if (dist < second_min_dist) {
                second_min_dist = dist;
            }
        }
    }

    if (best_cat == nullptr) {
        return result;
    }

    result.confidence = similarity_score(result.distance);

    ESP_LOGI(TAG, "Predict: best_class=%d, dist=%.4f, confidence=%.1f%%",
             best_class, result.distance, result.confidence);

    if (result.confidence >= 80.0f) {
        result.class_id = best_class;
    }

    return result;
}

int SelfLearningClassifier::get_class_count() const
{
    return (int)m_categories.size();
}

int SelfLearningClassifier::get_feat_dim() const
{
    return m_feat_dim;
}

SelfLearningClassifier::CategoryData *SelfLearningClassifier::new_category()
{
    auto *cat = new CategoryData();
    cat->prototypes = (float **)heap_caps_calloc(m_max_prototypes, sizeof(float *), MALLOC_CAP_INTERNAL);
    cat->count = 0;
    cat->total_samples = 0;
    cat->avg_intra_dist = 0.0f;
    return cat;
}

void SelfLearningClassifier::free_category(CategoryData *cat)
{
    for (int i = 0; i < cat->count; i++) {
        heap_caps_free(cat->prototypes[i]);
    }
    heap_caps_free(cat->prototypes);
    delete cat;
}

float *SelfLearningClassifier::copy_feat(const float *src)
{
    float *dst = (float *)heap_caps_malloc(m_feat_dim * sizeof(float), MALLOC_CAP_SPIRAM);
    if (dst) {
        memcpy(dst, src, m_feat_dim * sizeof(float));
    }
    return dst;
}

void SelfLearningClassifier::normalize(float *feat, int dim)
{
    float norm = 0.0f;
    for (int i = 0; i < dim; i++) {
        norm += feat[i] * feat[i];
    }
    norm = dl::math::sqrt_newton(norm);
    if (norm > 0.0f) {
        for (int i = 0; i < dim; i++) {
            feat[i] /= norm;
        }
    }
}

float SelfLearningClassifier::euclidean_distance(const float *a, const float *b, int dim)
{
    float sum = 0.0f;
    for (int i = 0; i < dim; i++) {
        float d = a[i] - b[i];
        sum += d * d;
    }
    return dl::math::sqrt_newton(sum);
}

float SelfLearningClassifier::similarity_score(float distance)
{
    float score = (1.0f - (distance * distance) / 2.0f) * 100.0f;
    if (score < 0.0f) {
        return 0.0f;
    }
    if (score > 100.0f) {
        return 100.0f;
    }
    return score;
}

void SelfLearningClassifier::add_prototype(CategoryData *cat, const float *feat)
{
    if (cat->count < m_max_prototypes) {
        cat->prototypes[cat->count] = copy_feat(feat);
        cat->count++;
    } else {
        // Find the closest existing prototype
        float min_dist = FLT_MAX;
        int closest_idx = 0;
        for (int i = 0; i < cat->count; i++) {
            float dist = euclidean_distance(feat, cat->prototypes[i], m_feat_dim);
            if (dist < min_dist) {
                min_dist = dist;
                closest_idx = i;
            }
        }
        // Replace if the new sample adds diversity
        if (cat->avg_intra_dist > 0 && min_dist > cat->avg_intra_dist * 0.6f) {
            heap_caps_free(cat->prototypes[closest_idx]);
            cat->prototypes[closest_idx] = copy_feat(feat);
        }
    }

    // Update average intra-class distance
    if (cat->count > 1) {
        float sum = 0.0f;
        int n = 0;
        for (int i = 0; i < cat->count; i++) {
            for (int j = i + 1; j < cat->count; j++) {
                sum += euclidean_distance(cat->prototypes[i], cat->prototypes[j], m_feat_dim);
                n++;
            }
        }
        cat->avg_intra_dist = sum / n;
    }
    cat->total_samples++;
}
