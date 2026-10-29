电子货架标签 Profile
==============================
:link_to_translation:`en:[English]`

**电子货架标签 Profile** (Electronic Shelf Label Profile, ESLP) 在 ``ble_conn_mgr`` 与 ESL Service 之上实现 ESL Tag 与 Access Point 角色。

Tag 角色 (``CONFIG_BLE_ESL_PROFILE``):

- 注册 ESL Service
- 状态：Unassociated、Configuring、Associated、Updating、Synchronized
- 关联特性写入与生命周期 ECP 命令
- 可选 PAST / PAwR 同步态

AP 角色 (``CONFIG_BLE_ESL_PROFILE_AP``):


- 对已连接 Tag 做 GATT 关联配置
- PAwR 广播控制、PAST 同步传输、按 subevent 排队命令

Tag 使用步骤：

#. 在 menuconfig 中启用 ``CONFIG_BLE_ESL_PROFILE``。
#. 依次调用 ``esp_ble_conn_init()``、``esp_ble_eslp_init()``、``esp_ble_conn_start()``。

AP 使用步骤：

#. 启用 ``CONFIG_BLE_ESL_PROFILE_AP`` 以及所需的 ``ble_conn_mgr`` 选项。
#. 依次调用 ``esp_ble_conn_init()``、``esp_ble_eslp_ap_init()``、``esp_ble_conn_start()``。
#. 发现完成后调用 ``esp_ble_eslp_ap_configure_tag()``。

示例
--------------

:example:`bluetooth/ble_profiles/ble_esl/ble_esl_tag`。

:example:`bluetooth/ble_profiles/ble_esl/ble_esl_ap`。

API 参考
-----------------

.. include-build-file:: inc/esp_eslp_tag.inc

.. include-build-file:: inc/esp_eslp_ap.inc
