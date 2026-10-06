/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "esp_display_present_endpoint.h"

#include <string.h>

#include "esp_attr.h"
#include "esp_display_present_gram.h"
#include "esp_display_present_fb.h"
#include "esp_display_present_te_compose.h"
#include "esp_log.h"

static const char *TAG = "present_endpoint";

static uint16_t drawbuf_lines_for_bytes(
    uint16_t width,
    uint16_t height,
    uint8_t color_bytes,
    size_t bytes)
{
    size_t row_bytes = (size_t)width * color_bytes;
    if (row_bytes == 0 || bytes < row_bytes) {
        return 0;
    }
    size_t lines = bytes / row_bytes;
    if (lines > height) {
        lines = height;
    }
    return (uint16_t)lines;
}

static esp_display_present_contract_t classify_contract(
    const esp_display_present_profile_t *profile)
{
    if (profile == NULL ||
            profile->storage == ESP_DISPLAY_PRESENT_STORAGE_GRAM) {
        return ESP_DISPLAY_PRESENT_CONTRACT_PARTITION;
    }
    switch (profile->fb) {
    case ESP_DISPLAY_PRESENT_FB_REPAIR:
        return ESP_DISPLAY_PRESENT_CONTRACT_PARTITION;
    case ESP_DISPLAY_PRESENT_FB_FULL:
        return ESP_DISPLAY_PRESENT_CONTRACT_FULL;
    case ESP_DISPLAY_PRESENT_FB_DIRECT:
    case ESP_DISPLAY_PRESENT_FB_PERSISTENT:
    default:
        return ESP_DISPLAY_PRESENT_CONTRACT_DIRECT;
    }
}

static void init_base_caps(
    esp_display_presenter_caps_t *caps,
    const esp_display_presenter_config_t *config,
    const esp_display_present_target_info_t *target_info)
{
    esp_display_present_contract_t contract =
        classify_contract(&target_info->fb.profile);
    *caps = (esp_display_presenter_caps_t) {
        .contract = contract,
        .width = config->width,
        .height = config->height,
        .stride_bytes =
            (size_t)config->width * target_info->hw.color_bytes,
            .pixel_format = config->pixel_format,
            .drawbuf_bytes =
                contract == ESP_DISPLAY_PRESENT_CONTRACT_PARTITION &&
                target_info->drawbuf.lines != 0
                ? (size_t)target_info->drawbuf.lines *
                (size_t)config->width * target_info->hw.color_bytes
                : 0,
                .max_damage_areas = config->max_damage_areas,
    };
}

static void apply_fb_caps(
    esp_display_presenter_caps_t *presenter_caps,
    const esp_display_present_fb_caps_t *caps)
{
    presenter_caps->supports_coverage_areas = caps->supports_coverage_areas;
    presenter_caps->surface_retains_content = caps->surface_retains_content;
    presenter_caps->previous_surface_readable = caps->previous_surface_readable;
    presenter_caps->stride_bytes = caps->stride_bytes;
    presenter_caps->max_damage_areas = caps->max_damage_areas;
}

static esp_err_t create_partition_drawbuf(
    esp_display_present_endpoint_binding_t *binding,
    const esp_display_present_target_info_t *target_info,
    uint16_t lines,
    uint8_t buffer_count,
    bool in_psram)
{
    if (binding == NULL || target_info == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    if (binding->caps.contract != ESP_DISPLAY_PRESENT_CONTRACT_PARTITION) {
        return ESP_OK;
    }
    if (lines == 0) {
        lines = drawbuf_lines_for_bytes(
                    target_info->hw.width, target_info->hw.height,
                    target_info->hw.color_bytes,
                    binding->caps.drawbuf_bytes);
    }
    if (lines == 0 || buffer_count == 0 ||
            buffer_count > ESP_DISPLAY_PRESENT_DRAWBUF_MAX_BUFFERS) {
        return ESP_ERR_NO_MEM;
    }
    esp_err_t ret = esp_display_present_drawbuf_pool_alloc(
                        &binding->drawbuf_pool, target_info->hw.width, lines,
                        target_info->hw.color_bytes, buffer_count, in_psram);
    if (ret == ESP_ERR_NO_MEM && buffer_count > 1 &&
            target_info->drawbuf.optional_second) {
        ESP_LOGW(TAG, "no memory for a second draw buffer; using one");
        ret = esp_display_present_drawbuf_pool_alloc(
                  &binding->drawbuf_pool, target_info->hw.width, lines,
                  target_info->hw.color_bytes, 1, in_psram);
    }
    if (ret == ESP_OK) {
        binding->caps.drawbuf_bytes = binding->drawbuf_pool.bytes;
    }
    return ret;
}

static void binding_reset(esp_display_present_endpoint_binding_t *binding)
{
    if (binding != NULL) {
        esp_display_present_drawbuf_pool_free(&binding->drawbuf_pool);
        memset(binding, 0, sizeof(*binding));
    }
}

static esp_err_t gram_stop(void *endpoint)
{
    return esp_display_present_gram_stop(endpoint);
}

static esp_err_t gram_destroy(void *endpoint)
{
    return esp_display_present_gram_delete(endpoint);
}

static bool IRAM_ATTR gram_transfer_done_isr(void *endpoint)
{
    return esp_display_present_gram_notify_transfer_done_from_isr(endpoint);
}

static bool IRAM_ATTR no_frame_done_isr(
    void *endpoint,
    BaseType_t *need_yield)
{
    (void)endpoint;
    (void)need_yield;
    return false;
}

static uint64_t gram_completed_frame(const void *endpoint)
{
    return esp_display_present_gram_get_completed_frame(endpoint);
}

static uint64_t gram_last_submitted(const void *endpoint)
{
    return esp_display_present_gram_get_last_submitted_frame(endpoint);
}

static esp_err_t te_compose_stop(void *endpoint)
{
    return esp_display_present_te_compose_stop(endpoint);
}

static esp_err_t te_compose_destroy(void *endpoint)
{
    return esp_display_present_te_compose_delete(endpoint);
}

static bool IRAM_ATTR te_compose_transfer_done_isr(void *endpoint)
{
    return esp_display_present_te_compose_notify_transfer_done_from_isr(endpoint);
}

static uint64_t te_compose_completed_transfer(const void *endpoint)
{
    return esp_display_present_te_compose_get_completed_transfer_frame(endpoint);
}

static uint64_t te_compose_last_submitted(const void *endpoint)
{
    return esp_display_present_te_compose_get_last_submitted_frame(endpoint);
}

static uint64_t te_compose_completed_present(const void *endpoint)
{
    return esp_display_present_te_compose_get_completed_present_frame(endpoint);
}

static esp_err_t fb_stop(void *endpoint)
{
    return esp_display_present_fb_stop(endpoint);
}

static esp_err_t fb_finish_stop_after_callbacks(void *endpoint)
{
    return esp_display_present_fb_finish_stop_after_callbacks(endpoint);
}

static esp_err_t fb_destroy(void *endpoint)
{
    return esp_display_present_fb_delete(endpoint);
}

static bool IRAM_ATTR fb_transfer_done_isr(void *endpoint)
{
    return esp_display_present_fb_notify_transfer_done_from_isr(endpoint);
}

static bool IRAM_ATTR fb_frame_done_isr(
    void *endpoint,
    BaseType_t *need_yield)
{
    return esp_display_present_fb_notify_frame_done_from_isr(
               endpoint, need_yield);
}

static uint64_t fb_completed_transfer(const void *endpoint)
{
    return esp_display_present_fb_get_completed_transfer(endpoint);
}

static uint64_t fb_last_submitted(const void *endpoint)
{
    return esp_display_present_fb_get_last_submitted(endpoint);
}

static uint64_t fb_completed_present(const void *endpoint)
{
    return esp_display_present_fb_get_completed_present(endpoint);
}

static const present_endpoint_ops_t s_gram_endpoint_ops = {
    .name = "gram",
    .stop_after_facade_returns_tiles = false,
    .stop = gram_stop,
    .destroy = gram_destroy,
    .transfer_done_isr = gram_transfer_done_isr,
    .frame_done_isr = no_frame_done_isr,
    .last_submitted = gram_last_submitted,
    .completed_transfer = gram_completed_frame,
    .completed_present = gram_completed_frame,
};

static const present_endpoint_ops_t s_te_compose_endpoint_ops = {
    .name = "te_compose",
    .stop_after_facade_returns_tiles = true,
    .stop = te_compose_stop,
    .destroy = te_compose_destroy,
    .transfer_done_isr = te_compose_transfer_done_isr,
    .frame_done_isr = no_frame_done_isr,
    .last_submitted = te_compose_last_submitted,
    .completed_transfer = te_compose_completed_transfer,
    .completed_present = te_compose_completed_present,
};

static const present_endpoint_ops_t s_fb_endpoint_ops = {
    .name = "fb",
    .stop_after_facade_returns_tiles = false,
    .quiesce_present_tail = 1,
    .stop = fb_stop,
    .finish_stop_after_callbacks = fb_finish_stop_after_callbacks,
    .destroy = fb_destroy,
    .transfer_done_isr = fb_transfer_done_isr,
    .frame_done_isr = fb_frame_done_isr,
    .last_submitted = fb_last_submitted,
    .completed_transfer = fb_completed_transfer,
    .completed_present = fb_completed_present,
};

static const present_mode_ops_t *const s_fb_mode_ops[] = {
    [ESP_DISPLAY_PRESENT_CONTRACT_PARTITION] = &present_mode_fb_partition,
    [ESP_DISPLAY_PRESENT_CONTRACT_DIRECT] = &present_mode_fb_switch,
    [ESP_DISPLAY_PRESENT_CONTRACT_FULL] = &present_mode_fb_switch,
};

static const present_mode_ops_t *select_fb_mode(
    esp_display_present_contract_t contract)
{
    size_t index = (size_t)contract;
    return index < sizeof(s_fb_mode_ops) / sizeof(s_fb_mode_ops[0])
           ? s_fb_mode_ops[index] : NULL;
}

static bool mode_ops_is_complete(const present_mode_ops_t *ops)
{
    return ops != NULL && ops->name != NULL &&
           ops->acquire_region != NULL && ops->cancel_region != NULL &&
           ops->submit_region != NULL && ops->commit_frame != NULL;
}

static esp_err_t create_gram_endpoint(
    esp_display_present_target_t *target,
    const esp_display_presenter_config_t *config,
    const esp_display_present_target_info_t *target_info,
    esp_display_present_endpoint_binding_t *binding)
{
    const esp_display_present_pipeline_policy_t *pipeline =
        &target_info->pipeline;
    esp_err_t ret = create_partition_drawbuf(
                        binding, target_info, pipeline->slot_lines,
                        pipeline->slot_count, target_info->drawbuf.in_psram);
    esp_display_present_gram_endpoint_t *gram = NULL;
    if (ret == ESP_OK) {
        ret = esp_display_present_gram_create(
                  target, &binding->drawbuf_pool, config->transfer_timeout_ms,
                  &gram);
    }
    if (ret == ESP_OK) {
        binding->ops = &s_gram_endpoint_ops;
        binding->mode_ops = &present_mode_gram_dma;
        binding->mode_ctx = gram;
        binding->tracker = esp_display_present_gram_get_tracker(gram);
    }
    return ret;
}

static esp_err_t create_te_compose_endpoint(
    esp_display_present_target_t *target,
    const esp_display_presenter_config_t *config,
    const esp_display_present_target_info_t *target_info,
    esp_display_present_endpoint_binding_t *binding)
{
    const esp_display_present_pipeline_policy_t *pipeline =
        &target_info->pipeline;
    esp_err_t ret = create_partition_drawbuf(
                        binding, target_info, pipeline->slot_lines,
                        pipeline->slot_count, target_info->drawbuf.in_psram);
    esp_display_present_te_compose_t *te_compose = NULL;
    if (ret == ESP_OK) {
        ret = esp_display_present_te_compose_create(
                  target, config->max_damage_areas, &binding->drawbuf_pool,
                  config->transfer_timeout_ms,
                  config->pipeline_acquire_timeout_ms, &te_compose);
    }
    if (ret == ESP_OK) {
        binding->ops = &s_te_compose_endpoint_ops;
        binding->mode_ops = &present_mode_te_compose;
        binding->mode_ctx = te_compose;
        binding->tracker =
            esp_display_present_te_compose_get_tracker(te_compose);
    }
    return ret;
}

static esp_err_t create_fb_endpoint(
    esp_display_present_target_t *target,
    const esp_display_presenter_config_t *config,
    const esp_display_present_target_info_t *target_info,
    esp_display_present_endpoint_binding_t *binding)
{
    const present_mode_ops_t *mode_ops = select_fb_mode(binding->caps.contract);
    if (mode_ops == NULL) {
        return ESP_ERR_INVALID_STATE;
    }
    const esp_display_present_pipeline_policy_t *pipeline =
        &target_info->pipeline;
    esp_err_t ret = create_partition_drawbuf(
                        binding, target_info, pipeline->slot_lines,
                        pipeline->slot_count,
                        target_info->drawbuf.in_psram);
    if (ret != ESP_OK) {
        return ret;
    }
    const esp_display_present_fb_endpoint_config_t fb_config = {
        .target = target,
        .max_damage_areas = config->max_damage_areas,
        .pipeline_acquire_timeout_ms = config->pipeline_acquire_timeout_ms,
        .drawbuf_pool = binding->drawbuf_pool.count != 0
        ? &binding->drawbuf_pool : NULL,
    };
    esp_display_present_fb_endpoint_t *fb = NULL;
    ret = esp_display_present_fb_create(&fb_config, &fb);
    if (ret != ESP_OK) {
        return ret;
    }
    esp_display_present_fb_caps_t fb_caps;
    ret = esp_display_present_fb_get_caps(fb, &fb_caps);
    if (ret != ESP_OK) {
        (void)esp_display_present_fb_delete(fb);
        return ret;
    }
    apply_fb_caps(&binding->caps, &fb_caps);
    binding->ops = &s_fb_endpoint_ops;
    binding->mode_ops = mode_ops;
    binding->mode_ctx = fb;
    binding->tracker = esp_display_present_fb_get_tracker(fb);
    return ESP_OK;
}

esp_err_t esp_display_present_endpoint_bind(
    esp_display_present_target_t *target,
    const esp_display_presenter_config_t *config,
    esp_display_present_endpoint_binding_t *out_endpoint)
{
    if (out_endpoint != NULL) {
        memset(out_endpoint, 0, sizeof(*out_endpoint));
    }
    if (target == NULL || config == NULL || out_endpoint == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    const esp_display_present_target_info_t *target_info =
        esp_display_present_target_get_info(target);
    if (target_info == NULL || target_info->hw.color_bytes == 0) {
        return ESP_ERR_INVALID_STATE;
    }
    if (target_info->pipeline.commit ==
            ESP_DISPLAY_PRESENT_FRAME_COMMIT_SWITCH &&
            config->max_damage_areas == 0) {
        return ESP_ERR_INVALID_ARG;
    }

    init_base_caps(&out_endpoint->caps, config, target_info);
    esp_err_t ret;
    switch (target_info->pipeline.commit) {
    case ESP_DISPLAY_PRESENT_FRAME_COMMIT_WAIT_INFLIGHT:
        ret = create_gram_endpoint(target, config, target_info, out_endpoint);
        break;
    case ESP_DISPLAY_PRESENT_FRAME_COMMIT_TE_PUSH:
        ret = create_te_compose_endpoint(
                  target, config, target_info, out_endpoint);
        break;
    case ESP_DISPLAY_PRESENT_FRAME_COMMIT_SWITCH:
        ret = create_fb_endpoint(
                  target, config, target_info, out_endpoint);
        break;
    default:
        ret = ESP_ERR_INVALID_STATE;
        break;
    }
    if (ret != ESP_OK || out_endpoint->ops == NULL ||
            !mode_ops_is_complete(out_endpoint->mode_ops) ||
            out_endpoint->mode_ctx == NULL ||
            out_endpoint->tracker == NULL) {
        if (out_endpoint->ops != NULL && out_endpoint->mode_ctx != NULL) {
            (void)out_endpoint->ops->destroy(out_endpoint->mode_ctx);
        }
        binding_reset(out_endpoint);
        return ret != ESP_OK ? ret : ESP_ERR_INVALID_STATE;
    }
    return ESP_OK;
}
