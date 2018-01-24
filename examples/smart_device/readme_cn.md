[[EN]](readme_en.md) 

# 智能设备示例使用手册

## 1. 硬件准备
  * 开发板：ESP32-DevKitC 开发板
  * 路由器：可以连接外网（所有的数据交互必需通过阿里云服务器）
  * 手机：若需要使用热点配网，则需要内置 SIM 卡，且可连接 4G 移动网络（热点配网时，设备需要通过手机热点连接阿里云服务器完成的注册、绑定等过程）
  * 阿里智能厂测包：下载请点击[这里](https://open.aliplus.com/download/?spm=0.0.0.0.7OBTZm)

## 2. 相关配置
可以通过 `make menuconfig` 修改 Alink embed 工程的一些相关配置： 

  * 在 `IoT Solution settings -> IoT Components Management -> Platforms and Clouds -> ALINK ENABLE -> ALINK_SETTINGS` 下可以选择 Alink 的版本（embed 或 sds），并配置 Alink sdk 内部的一些关键参数，例如连接路由的超时时间，一些关键任务的优先级和使用的栈空间的大小，log 等级等等。

    <img src="../../documents/_static/example/alink_demo/alink_settings.png" width = "300" alt="alink_settings" align=center />

  * 在 `IoT Example -> smart_device` 下可以选择设备类型，并配置该设备的相关参数和 ALINK 设备信息。

    <img src="../../documents/_static/example/alink_demo/smart_device_config.png" width = "300" alt="smart_device_config" align=center />

如需运行当前 demo，请使用默认参数即可。

## 3. demo 使用流程

1. 打开“阿里智能”，登录您的淘宝账号（为了便于找到我们的设备，可开启“模组认证测试列表”）。接着，进入`环境切换`，并滑到最底部勾选`开启配网模组测试列表`选项；然后，退出“阿里智能”，并关闭该 APP 的进程；最后再重新打开“阿里智能”并登录。
    
    <img src="../../documents/_static/example/alink_demo/home.png" width = "300" alt="home" align=center />

    <img src="../../documents/_static/example/alink_demo/open_test_module_list.png" width = "300" alt="open_test_module_list" align=center />

2. 点击右上角 `+` 号，并选择`添加设备`，进入添加设备的界面。

    <img src="../../documents/_static/example/alink_demo/add_device.png" width = "300" alt="add_device" align=center />

3. 点击`分类查找`，可见设备类别下面仅有`模组认证`一个选项，这是由于我们之前已经将`开启配网模组测试列表`打开了，如果将这个选项关闭，这里就会显示所有支持的设备类别。

    <img src="../../documents/_static/example/alink_demo/find_device.png" width = "300" alt="find_device" align=center />

    <img src="../../documents/_static/example/alink_demo/device_sort.png" width = "300" alt="device_sort" align=center />

4. 点击`模组认证`即可看到下方设备列表，请从中选择`配网V3_热点配网_小智`。

    <img src="../../documents/_static/example/alink_demo/select_device.png" width = "300" alt="select_device" align=center />

5. 开发板上电，先按下 EN 开关，后短按 (1-5s) boot 按键，最后查看打印信息。若出现 “ENTER SAMARTCONFIG MODE” 的信息，即表示已进入配网模式；任何情况下，短按 (1-5s) boot 键即可随时重新进入配网模式。  
   > 注：进入一键配网后，若开发板在 60s （默认值，可修改）内没有收到 SSID 和 password，则认为`一键配网`超时，模块自动进入`热点配网`模式。

    <img src="../../documents/_static/example/alink_demo/esp32_devkit_c.png" width = "300" alt="esp32_devkit_c" align=center />

    <img src="../../documents/_static/example/alink_demo/start_config_log.png" width = "300" alt="start_config_log" align=center />

6. APP 端选择使用的路由器并输入密码，点击`搜索设备`进入配网状态。

    <img src="../../documents/_static/example/alink_demo/start_configure.png" width = "300" alt="start_configure" align=center />

    <img src="../../documents/_static/example/alink_demo/configuring.png" width = "300" alt="configuring" align=center />

7. 设备`联网成功`后将进入`等待激活`界面。此时，请按下开发板上的 boot 键，触发激活指令上报，进而开始激活设备。

    <img src="../../documents/_static/example/alink_demo/active_device.png" width = "300" alt="active_device" align=center />

8. `激活成功`后将会跳转到`开始使用`界面。用户可在该界面中修改`设备名称`，并将该设备分配到某个用户名下。接着，点击`开启设备`即可进入最终控制界面。此时，用户即可从界面中实现设备的开关和控制。

    <img src="../../documents/_static/example/alink_demo/enable_device.png" width = "300" alt="enable_device" align=center />

    <img src="../../documents/_static/example/alink_demo/control_panel.png" width = "300" alt="control_panel" align=center />

## 4. 热点配网

1. 当`一键配网`失败（超时）时，用户可选择`热点模式`进行配网。

    <img src="../../documents/_static/example/alink_demo/config_fail.png" width = "300" alt="config_fail" align=center />

2. 在`热点模式`下，手机将创建一个 SSID 为 “aha”，密码为 “12345678” 的热点。设备将不断搜索这个热点，一旦搜到即与该热点建立连接。

    <img src="../../documents/_static/example/alink_demo/device_connect_hotpot.png" width = "300" alt="device_connect_hotpot" align=center />

    <img src="../../documents/_static/example/alink_demo/connect_hotpot_success.png" width = "300" alt="connect_hotpot_success" align=center />

3. 这时，APP 会提示您选择后续使用的路由器，并输入相应的密码；APP 将通过手机热点将路由器的 SSID 和 password 发送给设备端。

    <img src="../../documents/_static/example/alink_demo/select_ap.png" width = "300" alt="select_ap" align=center />

    <img src="../../documents/_static/example/alink_demo/enter_passwort.png" width = "300" alt="enter_passwort" align=center />

4. 设备端成功接收到 SSID 和 password 后即与手机热点断开，并尝试连接指定路由器，后续的流程与`一键配网`模式一致，即首先连接路由，然后连接服务器，最后上报激活指令完成配网。

    <img src="../../documents/_static/example/alink_demo/configuring_hotpot.png" width = "300" alt="configuring_hotpot" align=center />

    <img src="../../documents/_static/example/alink_demo/active_device.png" width = "300" alt="active_device" align=center />