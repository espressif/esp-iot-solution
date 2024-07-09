BLE HCI 组件
==============================
:link_to_translation:`en:[English]`

BLE HCI 组件用于通过 VHCI 接口直接操作 BLE Controller 实现广播,扫描等功能。
相比于通过 Nimble 或 Bluedroid 协议栈发起广播和扫描，使用该组件有如下优点：
- 更少的内存占用
- 更小的固件尺寸
- 更快的初始化流程

BLE HCI 使用方法
-----------------
对于广播应用：
1.初始化 BLE HCI: 使用 :cpp:func:`ble_hci_init` 函数进行初始化。
2.设定本机随机地址（可选）： 如果需要使用随机地址作为广播地址，使用:cpp:func:`ble_hci_set_random_address`函数进行设定。
3.配置广播参数：使用 :cpp:func:`ble_hci_set_adv_param` 配置广播参数。
4.配置广播数据：使用 :cpp:func:`ble_hci_set_adv_data` 设定需要广播的数据内容。
5.开始广播： 使用 :cpp:func:`ble_hci_set_adv_enable`。

对于扫描应用：
1.初始化 BLE HCI: 使用 :cpp:func:`ble_hci_init` 函数进行初始化。
2.配置扫描参数：使用 :cpp:func:`ble_hci_set_scan_param` 配置扫描参数。
3.使能meta事件: 使用 :cpp:func:`ble_hci_enable_meta_event` 使能中断事件。
4.注册扫描事件函数: 使用 :cpp:func:`ble_hci_set_register_scan_callback` 注册中断事件。
5.开始扫描： 使用 :cpp:func:`ble_hci_set_scan_enable`。

API 参考
---------------------------------------------
.. include-build-file:: inc/ble_hci.inc