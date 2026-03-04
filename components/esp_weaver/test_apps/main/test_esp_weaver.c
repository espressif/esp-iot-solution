/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "esp_log.h"
#include "unity.h"
#include "unity_test_runner.h"
#include "esp_weaver.h"
#include "esp_weaver_standard_types.h"
#include "esp_weaver_priv.h"

static const char *TAG = "weaver_test";
static const esp_weaver_config_t s_test_config = ESP_WEAVER_CONFIG_DEFAULT();

static int s_bulk_write_count;

static esp_err_t test_bulk_write_cb(const esp_weaver_device_t *device, const esp_weaver_param_write_req_t write_req[],
                                    uint8_t count, void *priv_data, esp_weaver_write_ctx_t *ctx)
{
    s_bulk_write_count += count;
    ESP_LOGI(TAG, "Bulk write callback: count=%d", count);
    return ESP_OK;
}

/* ========== Node Lifecycle ========== */

TEST_CASE("test_node_init_deinit", "[weaver][init]")
{
    esp_weaver_node_t *node = esp_weaver_node_init(&s_test_config, "Test Node", "TestType");
    TEST_ASSERT_NOT_NULL(node);
    TEST_ASSERT_EQUAL(ESP_WEAVER_STATE_INIT_DONE, esp_weaver_get_state());

    const char *node_id = esp_weaver_get_node_id();
    TEST_ASSERT_NOT_NULL(node_id);
    TEST_ASSERT_GREATER_THAN(0, strlen(node_id));

    TEST_ASSERT_EQUAL(ESP_OK, esp_weaver_node_deinit(node));
    TEST_ASSERT_EQUAL(ESP_WEAVER_STATE_DEINIT, esp_weaver_get_state());
    TEST_ASSERT_NULL(esp_weaver_get_node_id());
}

TEST_CASE("test_node_singleton", "[weaver][init]")
{
    esp_weaver_node_t *node = esp_weaver_node_init(&s_test_config, "Test Node", "TestType");
    TEST_ASSERT_NOT_NULL(node);

    TEST_ASSERT_NULL(esp_weaver_node_init(&s_test_config, "Another", "AnotherType"));

    esp_weaver_node_deinit(node);
}

/* ========== Device Lifecycle ========== */

TEST_CASE("test_device_create_delete", "[weaver][device]")
{
    int priv = 42;
    esp_weaver_device_t *dev = esp_weaver_device_create("Light", ESP_WEAVER_DEVICE_LIGHTBULB, &priv);
    TEST_ASSERT_NOT_NULL(dev);
    TEST_ASSERT_EQUAL_STRING("Light", esp_weaver_device_get_name(dev));
    TEST_ASSERT_EQUAL(42, *(int *)esp_weaver_device_get_priv_data(dev));

    TEST_ASSERT_EQUAL(ESP_OK, esp_weaver_device_delete(dev));
}

TEST_CASE("test_device_duplicate_name_rejected", "[weaver][device]")
{
    esp_weaver_node_t *node = esp_weaver_node_init(&s_test_config, "Test Node", "TestType");
    TEST_ASSERT_NOT_NULL(node);

    esp_weaver_device_t *dev1 = esp_weaver_device_create("Light", ESP_WEAVER_DEVICE_LIGHTBULB, NULL);
    TEST_ASSERT_EQUAL(ESP_OK, esp_weaver_node_add_device(node, dev1));

    esp_weaver_device_t *dev2 = esp_weaver_device_create("Light", ESP_WEAVER_DEVICE_LIGHTBULB, NULL);
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, esp_weaver_node_add_device(node, dev2));

    esp_weaver_device_delete(dev2);
    esp_weaver_node_deinit(node);
}

TEST_CASE("test_device_remove", "[weaver][device]")
{
    esp_weaver_node_t *node = esp_weaver_node_init(&s_test_config, "Test Node", "TestType");
    TEST_ASSERT_NOT_NULL(node);

    esp_weaver_device_t *dev = esp_weaver_device_create("Light", ESP_WEAVER_DEVICE_LIGHTBULB, NULL);
    TEST_ASSERT_EQUAL(ESP_OK, esp_weaver_node_add_device(node, dev));
    TEST_ASSERT_EQUAL(ESP_OK, esp_weaver_node_remove_device(node, dev));
    TEST_ASSERT_EQUAL(ESP_ERR_NOT_FOUND, esp_weaver_node_remove_device(node, dev));

    esp_weaver_device_delete(dev);
    esp_weaver_node_deinit(node);
}

TEST_CASE("test_device_delete_while_in_node_fails", "[weaver][device]")
{
    esp_weaver_node_t *node = esp_weaver_node_init(&s_test_config, "Test Node", "TestType");
    TEST_ASSERT_NOT_NULL(node);

    esp_weaver_device_t *dev = esp_weaver_device_create("Light", ESP_WEAVER_DEVICE_LIGHTBULB, NULL);
    TEST_ASSERT_NOT_NULL(dev);
    TEST_ASSERT_EQUAL(ESP_OK, esp_weaver_node_add_device(node, dev));

    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_STATE, esp_weaver_device_delete(dev));

    TEST_ASSERT_EQUAL(ESP_OK, esp_weaver_node_remove_device(node, dev));
    TEST_ASSERT_EQUAL(ESP_OK, esp_weaver_device_delete(dev));
    esp_weaver_node_deinit(node);
}

/* ========== Parameter Lifecycle ========== */

TEST_CASE("test_param_create_all_types", "[weaver][param]")
{
    esp_weaver_param_t *p;

    p = esp_weaver_param_create("Power", ESP_WEAVER_PARAM_POWER,
                                esp_weaver_bool(true), PROP_FLAG_READ | PROP_FLAG_WRITE);
    TEST_ASSERT_NOT_NULL(p);
    TEST_ASSERT_EQUAL_STRING("Power", esp_weaver_param_get_name(p));
    TEST_ASSERT_EQUAL(WEAVER_VAL_TYPE_BOOLEAN, esp_weaver_param_get_val(p)->type);
    TEST_ASSERT_TRUE(esp_weaver_param_get_val(p)->val.b);
    esp_weaver_param_delete(p);

    p = esp_weaver_param_create("Brightness", ESP_WEAVER_PARAM_BRIGHTNESS,
                                esp_weaver_int(50), PROP_FLAG_READ | PROP_FLAG_WRITE);
    TEST_ASSERT_NOT_NULL(p);
    TEST_ASSERT_EQUAL(50, esp_weaver_param_get_val(p)->val.i);
    esp_weaver_param_delete(p);

    p = esp_weaver_param_create("CCT", ESP_WEAVER_PARAM_CCT,
                                esp_weaver_float(25.5f), PROP_FLAG_READ);
    TEST_ASSERT_NOT_NULL(p);
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 25.5f, esp_weaver_param_get_val(p)->val.f);
    esp_weaver_param_delete(p);

    p = esp_weaver_param_create("Status", "esp.param.status",
                                esp_weaver_str("idle"), PROP_FLAG_READ);
    TEST_ASSERT_NOT_NULL(p);
    TEST_ASSERT_EQUAL_STRING("idle", esp_weaver_param_get_val(p)->val.s);
    esp_weaver_param_delete(p);
}

TEST_CASE("test_param_ui_and_bounds", "[weaver][param]")
{
    esp_weaver_param_t *param = esp_weaver_param_create("Brightness", ESP_WEAVER_PARAM_BRIGHTNESS,
                                                        esp_weaver_int(50), PROP_FLAG_READ | PROP_FLAG_WRITE);
    TEST_ASSERT_NOT_NULL(param);

    TEST_ASSERT_EQUAL(ESP_OK, esp_weaver_param_add_ui_type(param, ESP_WEAVER_UI_SLIDER));
    TEST_ASSERT_EQUAL(ESP_OK, esp_weaver_param_add_bounds(param,
                                                          esp_weaver_int(0), esp_weaver_int(100), esp_weaver_int(1)));

    esp_weaver_param_delete(param);
}

TEST_CASE("test_param_update_int", "[weaver][param]")
{
    esp_weaver_param_t *param = esp_weaver_param_create("Brightness", ESP_WEAVER_PARAM_BRIGHTNESS,
                                                        esp_weaver_int(50), PROP_FLAG_READ | PROP_FLAG_WRITE);
    TEST_ASSERT_NOT_NULL(param);

    TEST_ASSERT_EQUAL(ESP_OK, esp_weaver_param_update(param, esp_weaver_int(75)));
    TEST_ASSERT_EQUAL(75, esp_weaver_param_get_val(param)->val.i);

    esp_weaver_param_delete(param);
}

TEST_CASE("test_param_update_string", "[weaver][param]")
{
    esp_weaver_param_t *param = esp_weaver_param_create("Status", "esp.param.status",
                                                        esp_weaver_str("idle"), PROP_FLAG_READ | PROP_FLAG_WRITE);
    TEST_ASSERT_NOT_NULL(param);

    TEST_ASSERT_EQUAL(ESP_OK, esp_weaver_param_update(param, esp_weaver_str("active")));
    TEST_ASSERT_EQUAL_STRING("active", esp_weaver_param_get_val(param)->val.s);

    esp_weaver_param_delete(param);
}

TEST_CASE("test_param_update_type_mismatch", "[weaver][param]")
{
    esp_weaver_param_t *param = esp_weaver_param_create("Brightness", ESP_WEAVER_PARAM_BRIGHTNESS,
                                                        esp_weaver_int(50), PROP_FLAG_READ | PROP_FLAG_WRITE);
    TEST_ASSERT_NOT_NULL(param);

    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, esp_weaver_param_update(param, esp_weaver_bool(true)));

    esp_weaver_param_delete(param);
}

TEST_CASE("test_param_update_out_of_bounds", "[weaver][param]")
{
    esp_weaver_param_t *param = esp_weaver_param_create("Brightness", ESP_WEAVER_PARAM_BRIGHTNESS,
                                                        esp_weaver_int(50), PROP_FLAG_READ | PROP_FLAG_WRITE);
    TEST_ASSERT_NOT_NULL(param);

    TEST_ASSERT_EQUAL(ESP_OK, esp_weaver_param_add_bounds(param, esp_weaver_int(0), esp_weaver_int(100), esp_weaver_int(1)));

    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, esp_weaver_param_update(param, esp_weaver_int(150)));
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, esp_weaver_param_update(param, esp_weaver_int(-10)));
    TEST_ASSERT_EQUAL(ESP_OK, esp_weaver_param_update(param, esp_weaver_int(0)));
    TEST_ASSERT_EQUAL(ESP_OK, esp_weaver_param_update(param, esp_weaver_int(100)));
    TEST_ASSERT_EQUAL(ESP_OK, esp_weaver_param_update(param, esp_weaver_int(50)));

    esp_weaver_param_delete(param);
}

TEST_CASE("test_param_update_out_of_bounds_float", "[weaver][param]")
{
    esp_weaver_param_t *param = esp_weaver_param_create("CCT", ESP_WEAVER_PARAM_CCT,
                                                        esp_weaver_float(50.0f), PROP_FLAG_READ | PROP_FLAG_WRITE);
    TEST_ASSERT_NOT_NULL(param);

    TEST_ASSERT_EQUAL(ESP_OK, esp_weaver_param_add_bounds(param, esp_weaver_float(0.0f), esp_weaver_float(100.0f), esp_weaver_float(0.0f)));

    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, esp_weaver_param_update(param, esp_weaver_float(150.0f)));
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, esp_weaver_param_update(param, esp_weaver_float(-10.0f)));
    TEST_ASSERT_EQUAL(ESP_OK, esp_weaver_param_update(param, esp_weaver_float(50.0f)));

    esp_weaver_param_delete(param);
}

/* ========== Device + Parameter ========== */

TEST_CASE("test_device_param_lookup", "[weaver][device][param]")
{
    esp_weaver_device_t *dev = esp_weaver_device_create("Light", ESP_WEAVER_DEVICE_LIGHTBULB, NULL);
    TEST_ASSERT_NOT_NULL(dev);

    esp_weaver_param_t *power = esp_weaver_param_create("Power", ESP_WEAVER_PARAM_POWER,
                                                        esp_weaver_bool(true), PROP_FLAG_READ | PROP_FLAG_WRITE);
    esp_weaver_param_t *brightness = esp_weaver_param_create("Brightness", ESP_WEAVER_PARAM_BRIGHTNESS,
                                                             esp_weaver_int(50), PROP_FLAG_READ | PROP_FLAG_WRITE);
    TEST_ASSERT_EQUAL(ESP_OK, esp_weaver_device_add_param(dev, power));
    TEST_ASSERT_EQUAL(ESP_OK, esp_weaver_device_add_param(dev, brightness));

    TEST_ASSERT_EQUAL_PTR(power, esp_weaver_device_get_param_by_name(dev, "Power"));
    TEST_ASSERT_EQUAL_PTR(brightness, esp_weaver_device_get_param_by_type(dev, ESP_WEAVER_PARAM_BRIGHTNESS));
    TEST_ASSERT_NULL(esp_weaver_device_get_param_by_name(dev, "NonExistent"));

    esp_weaver_device_delete(dev);
}

TEST_CASE("test_device_duplicate_param_rejected", "[weaver][device][param]")
{
    esp_weaver_device_t *dev = esp_weaver_device_create("Light", ESP_WEAVER_DEVICE_LIGHTBULB, NULL);
    TEST_ASSERT_NOT_NULL(dev);

    esp_weaver_param_t *p1 = esp_weaver_param_create("Power", ESP_WEAVER_PARAM_POWER,
                                                     esp_weaver_bool(true), PROP_FLAG_READ);
    TEST_ASSERT_EQUAL(ESP_OK, esp_weaver_device_add_param(dev, p1));

    esp_weaver_param_t *p2 = esp_weaver_param_create("Power", ESP_WEAVER_PARAM_POWER,
                                                     esp_weaver_bool(false), PROP_FLAG_READ);
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, esp_weaver_device_add_param(dev, p2));

    esp_weaver_param_delete(p2);
    esp_weaver_device_delete(dev);
}

TEST_CASE("test_device_primary_param", "[weaver][device][param]")
{
    esp_weaver_device_t *dev = esp_weaver_device_create("Light", ESP_WEAVER_DEVICE_LIGHTBULB, NULL);
    TEST_ASSERT_NOT_NULL(dev);

    esp_weaver_param_t *power = esp_weaver_param_create("Power", ESP_WEAVER_PARAM_POWER,
                                                        esp_weaver_bool(true), PROP_FLAG_READ | PROP_FLAG_WRITE);
    TEST_ASSERT_EQUAL(ESP_OK, esp_weaver_device_add_param(dev, power));
    TEST_ASSERT_EQUAL(ESP_OK, esp_weaver_device_assign_primary_param(dev, power));

    /* Param not in device should fail */
    esp_weaver_param_t *orphan = esp_weaver_param_create("Orphan", ESP_WEAVER_PARAM_POWER,
                                                         esp_weaver_bool(true), PROP_FLAG_READ);
    TEST_ASSERT_EQUAL(ESP_ERR_NOT_FOUND, esp_weaver_device_assign_primary_param(dev, orphan));

    esp_weaver_param_delete(orphan);
    esp_weaver_device_delete(dev);
}

/* ========== Full Workflow ========== */

TEST_CASE("test_full_workflow", "[weaver][integration]")
{
    esp_weaver_node_t *node = esp_weaver_node_init(&s_test_config, "Test Light", "Lightbulb");
    TEST_ASSERT_NOT_NULL(node);

    esp_weaver_device_t *dev = esp_weaver_device_create("Light", ESP_WEAVER_DEVICE_LIGHTBULB, NULL);
    TEST_ASSERT_NOT_NULL(dev);

    esp_weaver_param_t *power = esp_weaver_param_create("Power", ESP_WEAVER_PARAM_POWER,
                                                        esp_weaver_bool(true), PROP_FLAG_READ | PROP_FLAG_WRITE);
    TEST_ASSERT_EQUAL(ESP_OK, esp_weaver_param_add_ui_type(power, ESP_WEAVER_UI_TOGGLE));
    TEST_ASSERT_EQUAL(ESP_OK, esp_weaver_device_add_param(dev, power));

    esp_weaver_param_t *brightness = esp_weaver_param_create("Brightness", ESP_WEAVER_PARAM_BRIGHTNESS,
                                                             esp_weaver_int(50), PROP_FLAG_READ | PROP_FLAG_WRITE);
    TEST_ASSERT_EQUAL(ESP_OK, esp_weaver_param_add_ui_type(brightness, ESP_WEAVER_UI_SLIDER));
    TEST_ASSERT_EQUAL(ESP_OK, esp_weaver_param_add_bounds(brightness, esp_weaver_int(0), esp_weaver_int(100), esp_weaver_int(1)));
    TEST_ASSERT_EQUAL(ESP_OK, esp_weaver_device_add_param(dev, brightness));

    TEST_ASSERT_EQUAL(ESP_OK, esp_weaver_device_add_bulk_cb(dev, test_bulk_write_cb));
    TEST_ASSERT_EQUAL(ESP_OK, esp_weaver_node_add_device(node, dev));

    TEST_ASSERT_EQUAL(ESP_WEAVER_STATE_INIT_DONE, esp_weaver_get_state());
    TEST_ASSERT_NOT_NULL(esp_weaver_get_node_id());

    esp_weaver_node_deinit(node);
}

/* ========== Write Request Handling ========== */

TEST_CASE("test_handle_set_params", "[weaver][integration]")
{
    esp_weaver_node_t *node = esp_weaver_node_init(&s_test_config, "Test Light", "Lightbulb");
    TEST_ASSERT_NOT_NULL(node);

    esp_weaver_device_t *dev = esp_weaver_device_create("Light", ESP_WEAVER_DEVICE_LIGHTBULB, NULL);
    TEST_ASSERT_EQUAL(ESP_OK, esp_weaver_device_add_bulk_cb(dev, test_bulk_write_cb));

    esp_weaver_param_t *power = esp_weaver_param_create("Power", ESP_WEAVER_PARAM_POWER,
                                                        esp_weaver_bool(false), PROP_FLAG_READ | PROP_FLAG_WRITE);
    esp_weaver_param_t *brightness = esp_weaver_param_create("Brightness", ESP_WEAVER_PARAM_BRIGHTNESS,
                                                             esp_weaver_int(50), PROP_FLAG_READ | PROP_FLAG_WRITE);
    TEST_ASSERT_EQUAL(ESP_OK, esp_weaver_device_add_param(dev, power));
    TEST_ASSERT_EQUAL(ESP_OK, esp_weaver_device_add_param(dev, brightness));
    TEST_ASSERT_EQUAL(ESP_OK, esp_weaver_node_add_device(node, dev));

    s_bulk_write_count = 0;
    const char *json = "{\"Light\":{\"Power\":true,\"Brightness\":75}}";
    TEST_ASSERT_EQUAL(ESP_OK, esp_weaver_handle_set_params(json, strlen(json), ESP_WEAVER_REQ_SRC_LOCAL));
    TEST_ASSERT_EQUAL(2, s_bulk_write_count);

    esp_weaver_node_deinit(node);
}

TEST_CASE("test_changed_params_tracking", "[weaver][integration]")
{
    esp_weaver_node_t *node = esp_weaver_node_init(&s_test_config, "Test Light", "Lightbulb");
    TEST_ASSERT_NOT_NULL(node);

    esp_weaver_device_t *dev = esp_weaver_device_create("Light", ESP_WEAVER_DEVICE_LIGHTBULB, NULL);
    esp_weaver_param_t *brightness = esp_weaver_param_create("Brightness", ESP_WEAVER_PARAM_BRIGHTNESS,
                                                             esp_weaver_int(50), PROP_FLAG_READ | PROP_FLAG_WRITE);
    TEST_ASSERT_EQUAL(ESP_OK, esp_weaver_device_add_param(dev, brightness));
    TEST_ASSERT_EQUAL(ESP_OK, esp_weaver_node_add_device(node, dev));

    /* No changes yet */
    char *changed = esp_weaver_get_changed_node_params();
    TEST_ASSERT_NOT_NULL(changed);
    TEST_ASSERT_EQUAL_STRING("{}", changed);
    free(changed);

    /* Update and verify change is tracked */
    TEST_ASSERT_EQUAL(ESP_OK, esp_weaver_param_update(brightness, esp_weaver_int(75)));
    changed = esp_weaver_get_changed_node_params();
    TEST_ASSERT_NOT_NULL(changed);
    TEST_ASSERT_NOT_NULL(strstr(changed, "75"));
    free(changed);

    /* Flags cleared after get — should be empty again */
    changed = esp_weaver_get_changed_node_params();
    TEST_ASSERT_EQUAL_STRING("{}", changed);
    free(changed);

    esp_weaver_node_deinit(node);
}

/* ========== Memory Leak Tests ========== */

TEST_CASE("test_memory_leak_string_param", "[weaver][memory]")
{
    for (int i = 0; i < 10; i++) {
        esp_weaver_param_t *param = esp_weaver_param_create("Status", "esp.param.status",
                                                            esp_weaver_str("test_string_value"), PROP_FLAG_READ);
        TEST_ASSERT_NOT_NULL(param);
        TEST_ASSERT_EQUAL(ESP_OK, esp_weaver_param_update(param, esp_weaver_str("updated_value")));
        ESP_ERROR_CHECK(esp_weaver_param_delete(param));
    }
}

TEST_CASE("test_memory_leak_full_workflow", "[weaver][memory]")
{
    for (int i = 0; i < 5; i++) {
        esp_weaver_node_t *node = esp_weaver_node_init(&s_test_config, "Test Node", "TestType");
        TEST_ASSERT_NOT_NULL(node);

        esp_weaver_device_t *dev = esp_weaver_device_create("Light", ESP_WEAVER_DEVICE_LIGHTBULB, NULL);
        TEST_ASSERT_NOT_NULL(dev);

        esp_weaver_param_t *param = esp_weaver_param_create("Power", ESP_WEAVER_PARAM_POWER,
                                                            esp_weaver_bool(true), PROP_FLAG_READ | PROP_FLAG_WRITE);
        TEST_ASSERT_NOT_NULL(param);
        ESP_ERROR_CHECK(esp_weaver_device_add_param(dev, param));
        ESP_ERROR_CHECK(esp_weaver_node_add_device(node, dev));
        ESP_ERROR_CHECK(esp_weaver_node_deinit(node));
    }
}

/* ========== Local Control Tests ========== */

TEST_CASE("test_local_ctrl_pop_validation", "[weaver][local_ctrl]")
{
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, esp_weaver_local_ctrl_set_pop(""));
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, esp_weaver_local_ctrl_set_pop("123456789"));
    TEST_ASSERT_EQUAL(ESP_OK, esp_weaver_local_ctrl_set_pop("12345678"));
    TEST_ASSERT_EQUAL(ESP_OK, esp_weaver_local_ctrl_set_pop(NULL));
}

/* ========== Test Runner ========== */

void app_main(void)
{
    printf("Press ENTER to see the list of tests.\n");
    unity_run_menu();
}
