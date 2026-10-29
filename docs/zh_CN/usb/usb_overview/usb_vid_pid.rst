USB VID 和 PID
--------------

:link_to_translation:`en:[English]`

VID 和 PID 是 USB 设备的唯一标识符，用于区分不同的 USB 设备。一般 VID 和 PID 由 USB-IF 分配，USB-IF 是 USB 设备的标准制定者。

以下情况您可以免申请 VID 和 PID
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

* 如果您的产品使用的是 USB Host 模式，您不需要申请 VID 和 PID。
* 如果您的产品使用的是 USB Device 模式，准备使用乐鑫的 VID（0x303A）, 并且基于 TinyUSB 协议栈开发 USB 标准设备，那您不需要申请 PID，使用 TinyUSB 默认 PID 即可。

申请 VID 和 PID
^^^^^^^^^^^^^^^

如果您使用的产品需要使用 USB 设备模式，您可按照以下过程申请 VID (Vendor ID) 或 PID (Product ID)。


* 如果您的产品需要使用 USB-IF 分配的 VID，您需要先 `注册成为 USB-IF 成员 <https://www.usb.org/members>`_，然后按照 USB-IF 的流程申请 VID 和 PID。
* 如果您的产品考虑使用乐鑫的 VID，您可以直接申请 PID（免费）, 申请流程请参考 `申请乐鑫 PID <https://github.com/espressif/usb-pids/blob/main/README.md>`_\ 。

注意：使用乐鑫的 VID 和 TinyUSB 的默认 PID, 并不意味着您的产品符合 USB 规范，您仍然需要进行 USB 认证。

USB 认证
^^^^^^^^^^^^^^^^^^^^^^^^^^^^

USB 认证是由 USB Implementers Forum（USB-IF）进行管理的，旨在确保产品符合 USB 规范，以保证设备之间的互操作性和兼容性。

USB 认证是一个可选的过程，但在以下情况下必须进行产品认证：


* 如果产品标榜自己符合 USB 规范并使用官方 `USB 标志 <https://www.usb.org/logo-license>`_\ 。
* 如果产品打算使用 USB 标志、商标或宣传资料中提及 USB 认证。

具体认证流程和要求，请查阅 https://www.usb.org 或联系 `USB-IF 授权测试实验室 <https://www.usb.org/labs>`_\ 。

此外，关于 USB 信号指令测试的详细步骤与固件，请参考 :doc:`USB Signal Quality Test <./usb_signal_quality>`
