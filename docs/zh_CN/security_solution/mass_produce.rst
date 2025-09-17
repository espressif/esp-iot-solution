量产方案
=========

本文档集合了用于乐鑫芯片量产的多种工具，包括烧录、测试、治具规范、配网及射频工具等内容。

**Flash 下载工具**
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

该工具面向生产环节，用于将量产固件高效烧录到芯片 Flash，支持多芯片并行烧录和多种烧录参数配置。它解决了量产过程中固件更新、批量烧录、自动化测试等问题，提升生产效率，降低人为失误。适用于产线固件部署、批量升级、调试流程。参考 `ESP32 生产阶段 <https://docs.espressif.com/projects/esp-test-tools/zh_CN/latest/esp32/production_stage/index.html>`_。


.. image:: https://docs.espressif.com/projects/esp-test-tools/zh_CN/latest/esp32/_images/factorymultidownload_interface.png
    :alt: Factory Multi Download Interface
    :align: center

**乐鑫产测工具**
~~~~~~~~~~~~~~~~~~~~

该工具是一拖四的产线测试上位机，采用"RAM 运行测试"方式，支持自动化配置和执行测试流程。功能包括 RF 性能测试、GPIO 导通测试、固件版本校验、Flash 读写/ID 测试等，能够自动生成测试日志和统计报表，并支持测试 bin、阈值和系统配置管理。它解决了产线测试效率低、人工操作繁琐、数据统计难等问题，适用于大批量生产测试、产线质量管控。参考 `ESP32 产测指南-产测工具 <https://docs.espressif.com/projects/esp-test-tools/zh_CN/latest/esp32/production_stage/tools/esp_production_testing_guide.html#production-testing-tool>`_。


.. image:: https://docs.espressif.com/projects/esp-test-tools/zh_CN/latest/esp32/_images/finish.png
    :alt: Factory Test UI Tool 测试完成界面
    :align: center

**模组冶具制作规范**
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

该规范指导产测治具的结构设计与制作工艺，确保测试夹具在量产过程中具备一致性和可靠性。通过标准化治具设计，减少因治具差异导致的测试误差和不良品率，提升测试效率和产品质量。适用于量产测试夹具开发、产线治具选型。参考 `ESP32 生产阶段 <https://docs.espressif.com/projects/esp-test-tools/zh_CN/latest/esp32/production_stage/index.html>`_。


.. image:: https://docs.espressif.com/projects/esp-test-tools/zh_CN/latest/esp32/_images/test_fixture_structure_cn.png
    :alt: 测试夹具结构示意图
    :align: center

**Matter QR 二维码生成工具**
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

该工具用于生成 Matter 设备的配网二维码，支持批量生成和自定义参数配置。它解决了产线设备快速入网、自动化配网、批量验证等问题，提升量产设备的联网效率和一致性。适用于 Matter 设备生产、产线配网、入网验证等场景。参考 `ESP32 生产阶段 <https://docs.espressif.com/projects/esp-test-tools/zh_CN/latest/esp32/production_stage/index.html>`_。


.. image:: https://docs.espressif.com/projects/esp-test-tools/zh_CN/latest/esp32/_images/ui_main.png
    :alt: Matter QR 二维码生成工具主界面
    :align: center

**EspRFTestTool 工具包**
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

* EspRFTestTool：用于射频测试的上位机工具，支持 Wi‑Fi 非信令测试、蓝牙/低功耗蓝牙非信令测试、Wi‑Fi 自适应测试等。该工具集成串口配置、固件快速烧录、测试参数配置与状态日志窗口，能够帮助研发人员在芯片开发阶段快速验证关键 RF 指标，发现并定位射频性能问题，提升产品可靠性。适用于芯片初期调试、射频性能评估、认证前预检等场景。下载入口见 `EspRFTestTool 工具包 <https://docs.espressif.com/projects/esp-test-tools/zh_CN/latest/esp32/development_stage/index.html>`_。
* DownloadTool：用于将测试固件烧录到芯片，支持 RAM 和 Flash 两种烧录模式，并可指定烧录地址。该工具主要解决射频测试前固件快速部署的问题，确保测试环境一致性，减少人工操作失误。适用于 RF 测试准备、固件版本切换、批量烧录等需求。参考 `ESP32 工具包 <https://docs.espressif.com/projects/esp-test-tools/zh_CN/latest/esp32/development_stage/index.html>`_。
* PowerLimitTool：用于生成定制化的 phy_init_data 固件，支持灵活配置射频功率限制等参数。该工具帮助用户满足不同国家/地区的射频法规要求，提升产品合规性，并可配合 RF 测试与认证流程使用。适用于认证准备、特殊应用场景功率调整等。参考 `ESP32 工具包 <https://docs.espressif.com/projects/esp-test-tools/zh_CN/latest/esp32/development_stage/index.html>`_。

.. image:: https://docs.espressif.com/projects/esp-test-tools/zh_CN/latest/esp32/_images/powerlimittool_rf_test_setting.png
    :alt: PowerLimitTool RF 测试设置界面
    :align: center

