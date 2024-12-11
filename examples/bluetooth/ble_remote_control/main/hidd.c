/*
 * SPDX-FileCopyrightText: 2023-2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file hidd.c
 * @brief HID Device
 *
 * @details This files defines the HID Device attributes
 *
*/
#include "hidd.h"

#define HID_DEMO_TAG "HID_RC"

// Store handlers for each attribute, needed for reading/writing/notifying
// Current implementation uses a struct with 3 arrays, but up to user to decide how to store handles
handle_table_t handle_table;

client_info_t client;
extern bool is_connected; // Need this here in the case of disconnection

// Characteristic Values
static uint8_t val_batt_level = 0;
// 0% battery level (assume battery not present, for user to modify if need)
static uint8_t val_bat_cccd[] = {0x01, 0x00};
// CCCD allows the Central Device to enable/disable notifications/indications
// 0x0001: bit 0 - to enable Notifications, bit 1 - to enable indications
// The endianness of the value is little endian (right to left), hence the first byte is 0x01 for notifications
static const uint8_t val_manu_name_str[] = {'E', 's', 'p', 'r', 'e', 's', 's', 'i', 'f'};
// UTF-8, big endian (left to right)
static const uint8_t val_pnp_id[] = {0x02, 0x3A, 0x30, 0x64, 0x00, 0x01, 0x00};
// The fields are in little endian (right to left for each field below)
//  Vendor ID Source: USB-IF (0x02), Vendor ID: Espressif Inc (0x303A), Product ID: ESP32 (0x0064), Product Version: 1 (0x0001)
static const uint8_t val_hid_info[] = {0x11, 0x01, 0x00, 0x00};
// USB HID version: 1.11 (0x0111), CountryCode: Not localized (0x00), Flags: No Flags (0x00)
static const uint8_t val_report_map[] = {
    0x05, 0x01,        // Usage Page (Generic Desktop Ctrls)
    0x09, 0x04,        // Usage (Joystick)
    0xA1, 0x01,        // Collection (Application)
    0x05, 0x01,        // Usage Page (Generic Desktop Ctrls)
    0x09, 0x01,        // Usage (Pointer)
    0xA1, 0x00,        // Collection (Physical)
    0x15, 0x00,        // Logical Minimum (0)
    0x26, 0xFF, 0x00,  // Logical Maximum (255)
    0x75, 0x08,        // Report Size (8)
    0x95, 0x02,        // Report Count (2)
    0xA4,              // Push
    0x09, 0x30,        // Usage (X)
    0x09, 0x31,        // Usage (Y)
    0x81, 0x02,        // Input (Data,Var,Abs,No Wrap,Linear,Preferred State,No Null Position)
    0x09, 0x39,        // Usage (Hat switch)
    0x15, 0x00,        // Logical Minimum (0)
    0x25, 0x03,        // Logical Maximum (3)
    0x35, 0x00,        // Physical Minimum (0)
    0x46, 0x0E, 0x01,  // Physical Maximum (270)
    0x65, 0x40,        // Unit (Length: Degrees)
    0x95, 0x01,        // Report Count (1)
    0x75, 0x04,        // Report Size (4)
    0x81, 0x42,        // Input (Data,Var,Abs,No Wrap,Linear,Preferred State,Null State)
    0x15, 0x00,        // Logical Minimum (0)
    0x25, 0x01,        // Logical Maximum (1)
    0x95, 0x02,        // Report Count (2)
    0x75, 0x01,        // Report Size (1)
    0x05, 0x09,        // Usage Page (Button)
    0x19, 0x01,        // Usage Minimum (0x01)
    0x29, 0x02,        // Usage Maximum (0x02)
    0x65, 0x00,        // Unit (None)
    0x81, 0x02,        // Input (Data,Var,Abs,No Wrap,Linear,Preferred State,No Null Position)
    0xC0,              // End Collection
    0x19, 0x03,        // Usage Minimum (0x03)
    0x29, 0x04,        // Usage Maximum (0x04)
    0x81, 0x02,        // Input (Data,Var,Abs,No Wrap,Linear,Preferred State,No Null Position)
    0xB4,              // Pop
    0x09, 0x33,        // Usage (Rx)
    0x95, 0x01,        // Report Count (1)
    0x81, 0x02,        // Input (Data,Var,Abs,No Wrap,Linear,Preferred State,No Null Position)
    0xC0,              // End Collection
};
// Report Map: defines the format of the data returned when the HID device is queried for output data
// Above report map is slightly modified from the one in the HID specification
// Up to user to customize implementation
static uint8_t val_hid_cntl_pt = 0x00;
// HID Control Pt allows the Central Device to send information to HID device
// Up to user to customize implementation
static uint8_t val_hid_report[] = {0x00, 0x00, 0x00, 0x00};
// HID report allows the HID device to send user input information to Central Device
// As described by the report map above
// byte 0: X-axis, 0 to 255
// byte 1: Y-axis, 0 to 255
// byte 2: bits 0 - 3 => hat switch, bits 4 - 7 => buttons
// byte 3: throttle
static uint8_t val_hid_cccd[] = {0x01, 0x00};
// Refer to the comment for val_bat_cccd above
static const uint8_t val_report_ref[] = {0x00, 0x01};
// Report ID: 0, not used (0x00), Report Type: Input (0x01)

// This is derived from the report map as defined above, index for the user input report
enum {
    IDX_HID_REPORT_JOYSTICK_X = 0,
    IDX_HID_REPORT_JOYSTICK_Y = 1,
    IDX_HID_REPORT_SWITCH_BUTTONS = 2,
    IDX_HID_REPORT_THROTTLE = 3,
};

/** (For easy reference)
 * esp_gatts_attr_db_t
 *  - esp_attr_control_t
 *      - uint8_t auto_rsp
 *  - esp_attr_desc_t
 *      - uint16_t uuid_length
 *      - uint16_t *uuid_p
 *      - uint16_t perm
 *      - uint16_t max_length
 *      - uint16_t length
 *      - uint8_t *value
*/
// Attribute table for Battery Service
const esp_gatts_attr_db_t attr_tab_bat_svc[LEN_BAT_SVC] = {
    [IDX_BAT_SVC] = {   // Service declaration: to describe the properties of a service
        {ESP_GATT_AUTO_RSP},  // Attribute Response Control (esp_attr_control_t)
        {
            // Attribute description (esp_attr_desc_t)
            ESP_UUID_LEN_16,            // UUID length
            (uint8_t *) &UUID_PRI_SVC,  // UUID pointer
            ESP_GATT_PERM_READ,         // Permissions
            sizeof(uint16_t),           // Max length of element
            sizeof(UUID_BAT_SVC),       // Length of element
            (uint8_t *) &UUID_BAT_SVC,  // Pointer to element value
        },
    },
    [IDX_BAT_LVL_CHAR] = { // Characteristic declaration: to describe the properties of a characteristic
        {ESP_GATT_AUTO_RSP},
        {
            ESP_UUID_LEN_16,
            (uint8_t *) &UUID_CHAR_DECLARE,
            ESP_GATT_PERM_READ,
            sizeof(uint8_t),
            sizeof(CHAR_PROP_READ_NOTIFY),
            (uint8_t *) &CHAR_PROP_READ_NOTIFY,
        },
    },
    [IDX_BAT_LVL_VAL] = { // Characteristic value: to contain the actual value of a characteristic
        {ESP_GATT_AUTO_RSP},
        {
            ESP_UUID_LEN_16,
            (uint8_t *) &UUID_BAT_LVL_VAL,
            ESP_GATT_PERM_READ,
            sizeof(uint8_t),
            sizeof(val_batt_level),
            (uint8_t *) &val_batt_level,
        },
    },
    [IDX_BAT_LVL_DESCR_CCCD] = { // Characteristic descriptor: to describe the properties of a characteristic value
        {ESP_GATT_AUTO_RSP},
        {
            ESP_UUID_LEN_16,
            (uint8_t *) &UUID_HID_CLIENT_CONFIG_DESCR,
            ESP_GATT_PERM_READ | ESP_GATT_PERM_WRITE,
            sizeof(val_bat_cccd),
            sizeof(val_bat_cccd),
            (uint8_t *) &val_bat_cccd,
        },
    },
};

// Attribute table for Device Information Service
const esp_gatts_attr_db_t attr_tab_dev_info_svc[LEN_DEV_INFO_SVC] = {
    [IDX_DEV_INFO_SVC] = {
        {ESP_GATT_AUTO_RSP},
        {
            ESP_UUID_LEN_16,
            (uint8_t *) &UUID_PRI_SVC,
            ESP_GATT_PERM_READ,
            sizeof(uint16_t),
            sizeof(UUID_DEV_INFO_SVC),
            (uint8_t *) &UUID_DEV_INFO_SVC,
        },
    },
    [IDX_DEV_INFO_MANUFACTURER_NAME_CHAR] = {
        {ESP_GATT_AUTO_RSP},
        {
            ESP_UUID_LEN_16,
            (uint8_t *) &UUID_CHAR_DECLARE,
            ESP_GATT_PERM_READ,
            sizeof(uint8_t),
            sizeof(CHAR_PROP_READ),
            (uint8_t *) &CHAR_PROP_READ,
        },
    },
    [IDX_DEV_INFO_MANUFACTURER_NAME_VAL] = {
        {ESP_GATT_AUTO_RSP},
        {
            ESP_UUID_LEN_16,
            (uint8_t *) &UUID_MANU_NAME_VAL,
            ESP_GATT_PERM_READ,
            sizeof(val_manu_name_str),
            sizeof(val_manu_name_str),
            (uint8_t *) &val_manu_name_str,
        },
    },
    [IDX_DEV_INFO_PNP_ID_CHAR] = {
        {ESP_GATT_AUTO_RSP},
        {
            ESP_UUID_LEN_16,
            (uint8_t *) &UUID_CHAR_DECLARE,
            ESP_GATT_PERM_READ,
            sizeof(uint8_t),
            sizeof(CHAR_PROP_READ),
            (uint8_t *) &CHAR_PROP_READ,
        },
    },
    [IDX_DEV_INFO_PNP_ID_VAL] = {
        {ESP_GATT_AUTO_RSP},
        {
            ESP_UUID_LEN_16,
            (uint8_t *) &UUID_PNP_ID_VAL,
            ESP_GATT_PERM_READ,
            sizeof(val_pnp_id),
            sizeof(val_pnp_id),
            (uint8_t *) &val_pnp_id,
        },
    },
};

// Attribute table for HID Service
const esp_gatts_attr_db_t attr_tab_hid_svc[LEN_HID_SVC] = {
    [IDX_HID_SVC] = {
        {ESP_GATT_AUTO_RSP},
        {
            ESP_UUID_LEN_16,
            (uint8_t *) &UUID_PRI_SVC,
            ESP_GATT_PERM_READ,
            sizeof(uint16_t),
            sizeof(UUID_HID_SVC),
            (uint8_t *) &UUID_HID_SVC,
        },
    },
    [IDX_HID_INFO_CHAR] = {
        {ESP_GATT_AUTO_RSP},
        {
            ESP_UUID_LEN_16,
            (uint8_t *) &UUID_CHAR_DECLARE,
            ESP_GATT_PERM_READ,
            sizeof(uint8_t),
            sizeof(CHAR_PROP_READ),
            (uint8_t *) &CHAR_PROP_READ,
        },
    },
    [IDX_HID_INFO_VAL] = {
        {ESP_GATT_AUTO_RSP},
        {
            ESP_UUID_LEN_16,
            (uint8_t *) &UUID_HID_INFO_VAL,
            ESP_GATT_PERM_READ,
            sizeof(val_hid_info),
            sizeof(val_hid_info),
            (uint8_t *) &val_hid_info,
        },
    },
    [IDX_HID_REPORT_MAP_CHAR] = {
        {ESP_GATT_AUTO_RSP},
        {
            ESP_UUID_LEN_16,
            (uint8_t *) &UUID_CHAR_DECLARE,
            ESP_GATT_PERM_READ,
            sizeof(uint8_t),
            sizeof(CHAR_PROP_READ),
            (uint8_t *) &CHAR_PROP_READ,
        },
    },
    [IDX_HID_REPORT_MAP_VAL] = {
        {ESP_GATT_AUTO_RSP},
        {
            ESP_UUID_LEN_16,
            (uint8_t *) &UUID_HID_REPORT_MAP_VAL,
            ESP_GATT_PERM_READ,
            sizeof(val_report_map),
            sizeof(val_report_map),
            (uint8_t *) &val_report_map,
        },
    },
    [IDX_HID_REPORT_MAP_DESCR_EXT_REF] = {
        {ESP_GATT_AUTO_RSP},
        {
            ESP_UUID_LEN_16,
            (uint8_t *) &UUID_HID_EXT_REPORT_REF_DESCR,
            ESP_GATT_PERM_READ,
            sizeof(UUID_BAT_LVL_VAL),
            sizeof(UUID_BAT_LVL_VAL),
            (uint8_t *) &UUID_BAT_LVL_VAL,
        },
    },
    [IDX_HID_CTNL_PT_CHAR] = {
        {ESP_GATT_AUTO_RSP},
        {
            ESP_UUID_LEN_16,
            (uint8_t *) &UUID_CHAR_DECLARE,
            ESP_GATT_PERM_READ,
            sizeof(uint8_t),
            sizeof(CHAR_PROP_READ),
            (uint8_t *) &CHAR_PROP_READ,
        },
    },
    [IDX_HID_CTNL_PT_VAL] = {
        {ESP_GATT_AUTO_RSP},
        {
            ESP_UUID_LEN_16,
            (uint8_t *) &UUID_HID_CTNL_PT_VAL,
            ESP_GATT_PERM_WRITE,
            sizeof(uint8_t),
            sizeof(val_hid_cntl_pt),
            (uint8_t *) &val_hid_cntl_pt,
        },
    },
    [IDX_HID_REPORT_CHAR] = {
        {ESP_GATT_AUTO_RSP},
        {
            ESP_UUID_LEN_16,
            (uint8_t *) &UUID_CHAR_DECLARE,
            ESP_GATT_PERM_READ,
            sizeof(uint8_t),
            sizeof(CHAR_PROP_READ_NOTIFY),
            (uint8_t *) &CHAR_PROP_READ_NOTIFY,
        },
    },
    [IDX_HID_REPORT_VAL] = {
        {ESP_GATT_AUTO_RSP},
        {
            ESP_UUID_LEN_16,
            (uint8_t *) &UUID_HID_REPORT_VAL,
            ESP_GATT_PERM_READ,
            sizeof(val_hid_report),
            sizeof(val_hid_report),
            (uint8_t *) &val_hid_report,
        },
    },
    [IDX_HID_REPORT_DESCR_CCCD] = {
        {ESP_GATT_AUTO_RSP},
        {
            ESP_UUID_LEN_16,
            (uint8_t *) &UUID_HID_CLIENT_CONFIG_DESCR,
            ESP_GATT_PERM_READ | ESP_GATT_PERM_WRITE,
            sizeof(val_hid_cccd),
            sizeof(val_hid_cccd),
            (uint8_t *) &val_hid_cccd,
        },
    },
    [IDX_HID_REPORT_DESCR_RPT_REF] = {
        {ESP_GATT_AUTO_RSP},
        {
            ESP_UUID_LEN_16,
            (uint8_t *) &UUID_HID_REPORT_REF_DESCR,
            ESP_GATT_PERM_READ,
            sizeof(val_report_ref),
            sizeof(val_report_ref),
            (uint8_t *) &val_report_ref,
        },
    },
};

// HID Event Private Functions
void hid_register_event(esp_gatt_if_t gatts_if);
void hid_attribute_table_created_event(esp_ble_gatts_cb_param_t *param);
void hid_close_event(void);

void hid_event_callback(esp_gatts_cb_event_t event, esp_gatt_if_t gatts_if, esp_ble_gatts_cb_param_t *param)
{
    switch (event) {
    case ESP_GATTS_REG_EVT:
        ESP_LOGI(HID_DEMO_TAG, "Profile Registered, app_id %04x, status %d",
                 param->reg.app_id, param->reg.status);
        hid_register_event(gatts_if);
        break;
    case ESP_GATTS_CONNECT_EVT:
        ESP_LOGI(HID_DEMO_TAG, "Profile Receiving a new connection, conn_id %d", param->connect.conn_id);
        // Save the connection ID, need this for sending notification in send_user_input()
        client.connection_id = param->connect.conn_id;
        break;
    case ESP_GATTS_DISCONNECT_EVT:
        ESP_LOGI(HID_DEMO_TAG, "Profile Receiving a disconnection, conn_id %d", param->disconnect.conn_id);
        is_connected = false;
        break;
    case ESP_GATTS_CREAT_ATTR_TAB_EVT:
        if (param->add_attr_tab.status == ESP_GATT_OK) {
            ESP_LOGI(HID_DEMO_TAG, "Create attribute table successfully, table size = %d", param->add_attr_tab.num_handle);
            hid_attribute_table_created_event(param);
        } else {
            ESP_LOGE(HID_DEMO_TAG, "Create attribute table failed, error code = %x", param->add_attr_tab.status);
        }
        break;
    case ESP_GATTS_START_EVT:
        ESP_LOGI(HID_DEMO_TAG, "Start Service %s, service_handle %d",
                 (param->start.status == ESP_GATT_OK ? "Successful" : "Failed"), param->start.service_handle);
        break;
    case ESP_GATTS_READ_EVT:
        ESP_LOGI(HID_DEMO_TAG, "Read by Client, conn_id : %d, handle: %d, need response: %s",
                 param->read.conn_id, param->read.handle, param->read.need_rsp ? "yes" : "no");
        ESP_LOGI(HID_DEMO_TAG, BD_ADDR_2_STR(param->read.bda));
        break;
    case ESP_GATTS_CONF_EVT:
        ESP_LOGI(HID_DEMO_TAG, "Receive Confirm Event, status = %d, attr_handle %d", param->conf.status, param->conf.handle);
        break;
    case ESP_GATTS_MTU_EVT:
        ESP_LOGI(HID_DEMO_TAG, "ESP_GATTS_MTU_EVT, MTU %d", param->mtu.mtu);
        break;
    case ESP_GATTS_CLOSE_EVT:
        ESP_LOGI(HID_DEMO_TAG, "Closing Remote Connection");
        hid_close_event(); // Trigger ESP_GATTS_STOP_EVT
        break;
    case ESP_GATTS_STOP_EVT:
        ESP_LOGI(HID_DEMO_TAG, "Stopping service %d %s", param->stop.service_handle,
                 (param->stop.status == ESP_GATT_OK) ? "Successful" : "Failed");
        esp_ble_gatts_delete_service(param->stop.service_handle); // Trigger ESP_GATTS_DELETE_EVT
        break;
    case ESP_GATTS_DELETE_EVT:
        ESP_LOGI(HID_DEMO_TAG, "Delete service handle %d %s", param->del.service_handle,
                 (param->del.status == ESP_GATT_OK) ? "Successful" : "Failed");
        break;
    case ESP_GATTS_UNREG_EVT:
        ESP_LOGI(HID_DEMO_TAG, "GATTS Unregistered");
        break;
    default:
        // Refer to ESP-IDF esp_gatts_api.h for the event definitions
        ESP_LOGI(HID_DEMO_TAG, "Unhandled HID Event, event %d", event);
        break;
    }
    return;
}

void hid_register_event(esp_gatt_if_t gatts_if)
{
    // Trigger ESP_GATTS_CREAT_ATTR_TAB_EVT
    esp_err_t ret = esp_ble_gatts_create_attr_tab(attr_tab_bat_svc, gatts_if, LEN_BAT_SVC, INST_ID_BAT_SVC);
    if (ret) {
        ESP_LOGE(HID_DEMO_TAG, "Create Battery Service attribute table failed, error code = %x", ret);
    }
    ret = esp_ble_gatts_create_attr_tab(attr_tab_dev_info_svc, gatts_if, LEN_DEV_INFO_SVC, INST_ID_DEV_INFO);
    if (ret) {
        ESP_LOGE(HID_DEMO_TAG, "Create Device Information Service attribute table failed, error code = %x", ret);
    }
    ret = esp_ble_gatts_create_attr_tab(attr_tab_hid_svc, gatts_if, LEN_HID_SVC, INST_ID_HID);
    if (ret) {
        ESP_LOGE(HID_DEMO_TAG, "Create HID Service attribute table failed, error code = %x", ret);
    }

    // Need this to send notification & to close the connection
    client.gatt_if = gatts_if;
}

/**
 * @brief Helper function for attribute table creation event to check the length of the attribute table
 * @param instance_id The instance ID of the service
 * @param len The length of the attribute table
 * @return true if the length is correct, false otherwise
 * @note Check against the length of each service's attribute table as defined in the macro LEN_XXX_SVC,
*/
bool is_correct_attr_table_len(int instance_id, int len)
{
    switch (instance_id) {
    case INST_ID_BAT_SVC:
        return len == LEN_BAT_SVC;
    case INST_ID_HID:
        return len == LEN_HID_SVC;
    case INST_ID_DEV_INFO:
        return len == LEN_DEV_INFO_SVC;
    default:
        return false;
    }
}

/**
 * @brief Helper function for attribute table creation event to copy the attribute handles into the corresponding handle table
*/
esp_err_t copy_attribute_handles(int instance_id, uint16_t *handles, int num_handle)
{
    switch (instance_id) {
    case INST_ID_BAT_SVC:
        memcpy(handle_table.handles_bat_svc, handles, num_handle * sizeof(uint16_t));
        client.bat_attr_handle = handles[IDX_BAT_LVL_VAL];
        return ESP_OK;
    case INST_ID_HID:
        memcpy(handle_table.handles_hid_svc, handles, num_handle * sizeof(uint16_t));
        // Need the attribute handle for the HID Report characteristic to send notifications
        client.attr_handle = handles[IDX_HID_REPORT_VAL];
        return ESP_OK;
    case INST_ID_DEV_INFO:
        memcpy(handle_table.handles_dev_info_svc, handles, num_handle * sizeof(uint16_t));
        return ESP_OK;
    default:
        return ESP_FAIL;
    }
}

/**
 * @brief Handle attribute table creation event
*/
void hid_attribute_table_created_event(esp_ble_gatts_cb_param_t *param)
{
    // Same service instance ID as the one passed into esp_ble_gatts_create_attr_tab
    ESP_LOGI(HID_DEMO_TAG, "Service Instance ID: %d, UUID: 0x%x", param->add_attr_tab.svc_inst_id, param->add_attr_tab.svc_uuid.uuid.uuid16);
    ESP_LOGI(HID_DEMO_TAG, "Attribute Table Length: %d", param->add_attr_tab.num_handle);

    if (!is_correct_attr_table_len(param->add_attr_tab.svc_inst_id, param->add_attr_tab.num_handle)) {
        ESP_LOGE(HID_DEMO_TAG, "Create attribute table abnormally, num_handle (%d) doesn't match instance_id (%d)", param->add_attr_tab.num_handle, param->add_attr_tab.svc_inst_id);
        return;
    }

    ESP_LOGI(HID_DEMO_TAG, "Create attribute table successfully, length = %d\n", param->add_attr_tab.num_handle);

    if (copy_attribute_handles(param->add_attr_tab.svc_inst_id, param->add_attr_tab.handles, param->add_attr_tab.num_handle) != ESP_OK) {
        ESP_LOGE(HID_DEMO_TAG, "Failed to copy attribute handles");
        return;
    }
    // Trigger ESP_GATTS_START_EVT, first element in add_attr_tab.handles is the service handle
    esp_ble_gatts_start_service(param->add_attr_tab.handles[0]);
}

/**
 * @brief Handle HID close event
*/
void hid_close_event(void)
{
    // Close each service
    // Each of the following triggers ESP_GATTS_STOP_EVT
    esp_ble_gatts_stop_service(handle_table.handles_bat_svc[IDX_BAT_SVC]);
    esp_ble_gatts_stop_service(handle_table.handles_hid_svc[IDX_HID_SVC]);
    esp_ble_gatts_stop_service(handle_table.handles_dev_info_svc[IDX_DEV_INFO_SVC]);
}

void set_hid_report_values(uint8_t joystick_x, uint8_t joystick_y, uint8_t buttons, uint8_t hat_switch, uint8_t throttle)
{
    // As described by the report map above
    // byte 0: X-axis, 0 to 255
    // byte 1: Y-axis, 0 to 255
    // byte 2: bits 0 - 3 => hat switch, bits 4 - 7 => buttons
    // byte 3: throttle
    val_hid_report[IDX_HID_REPORT_JOYSTICK_X] = joystick_x;
    val_hid_report[IDX_HID_REPORT_JOYSTICK_Y] = joystick_y;
    val_hid_report[IDX_HID_REPORT_SWITCH_BUTTONS] = hat_switch << 4;
    val_hid_report[IDX_HID_REPORT_SWITCH_BUTTONS] |= buttons;
    val_hid_report[IDX_HID_REPORT_THROTTLE] = throttle;
}

esp_err_t send_user_input(void)
{
    ESP_LOGI(HID_DEMO_TAG, "Send notif, params: gatt_if: 0x%x, conn_id: 0x%x, attr_handle: 0x%x, len: %d, values %x:%x:%x:%x",
             client.gatt_if, client.connection_id, client.attr_handle, sizeof(val_hid_report) / sizeof(val_hid_report[0]),
             val_hid_report[0], val_hid_report[1], val_hid_report[2], val_hid_report[3]);

    return esp_ble_gatts_send_indicate(
               client.gatt_if, // gatts_if
               client.connection_id,   // conn_id
               client.attr_handle,     // attr_handle
               sizeof(val_hid_report) / sizeof(val_hid_report[0]), // value_len
               val_hid_report, // value
               false   // need_confirm
           );
}

esp_err_t set_hid_battery_level(uint8_t value)
{
    if (value > 100) {
        value = 100;
    }

    val_batt_level = value;

    return esp_ble_gatts_send_indicate(
               client.gatt_if, // gatts_if
               client.connection_id,   // conn_id
               client.bat_attr_handle,     // attr_handle
               1, // value_len
               &val_batt_level, // value
               false   // need_confirm
           );
}
