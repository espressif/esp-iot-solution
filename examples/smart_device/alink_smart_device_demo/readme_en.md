[[中文]](readme_cn.md) 

# Smart Device Example User Guide

## 1. How to prepare the hardware environment
  * Development board: ESP32-DevKitC
  * Router: A router that can connect to the external network, since all data must be exchanged through the Aliyun server.
  * Mobile phone: If you want to use the `热点配网 (Hotspot Configuration)`, which means completing the configuration through the personal hotspot of your mobile phone, a SIM card for a 4G mobile network must be inserted on your mobile phone. The device will need to connect itself to the Aliyun server through the mobile phone's hotspot, and complete the registration, bonding and so on.
  * Aliyun App (beta): [Download it here](https://open.aliplus.com/download/?spm=0.0.0.0.7OBTZm)

## 2. How to configure parameters
The configuration of the Alink embed project can be modified at `make menuconfig`(Make) or `idf.py menuconfig`(CMake):

  * Go to `IoT Solution settings -> IoT Components Management -> Platforms and Clouds -> ALINK ENABLE -> ALINK_SETTINGS`, select the version of Alink (embed or SDS), and configure the key parameters of the Alink SDK, such as the connection timeout,  the priority level for key tasks, the size of the used stack, the log level and so on.

    <img src="../../documents/_static/example/alink_demo/alink_settings.png" width = "300" alt="alink_settings" align=center />

  * Go to `IoT Example - smart_device`, select the `Device type`, configure the device's parameters and the Alink parameters for this device.

    <img src="../../documents/_static/example/alink_demo/smart_device_config.png" width = "300" alt="smart_device_config" align=center />

Note：This is a smart light demo if use default parameters. If you want to use smart plug demo, you need to set `Device type` as `smart plug`.

## 3. How to complete the Auto Configuaration

1. Open `Alibaba Smart Living` and log in to your Taobao account.
2. Click the `+` sign in the upper right corner, and select `添加设备 (Add a Device)`.

    <img src="../../documents/_static/example/alink_demo/add_device.png" width = "300" alt="add_device" align=center />

3. Click `分类查找 (Search by Category)` to see all the supported device categories, and find the category named `照明 (Lighting)`.

    <img src="../../documents/_static/example/alink_demo/find_device.png" width = "300" alt="find_device" align=center />
    <img src="../../documents/_static/example/alink_demo/device_sort.png" width = "300" alt="device_sort" align=center />

4. Click `照明 (Lighting)` to see all the supported devices, and select `ESP-LIGHT-DEMO-V5`. The searching may take a while, because there are many test devices in the list.

    <img src="../../documents/_static/example/alink_demo/select_device.png" width = "300" alt="select_device" align=center />

5. Press the `EN` switch when the development board is powered up; then, press the `boot` button for 1 to 5 seconds, and view the print log. Check if the `ENTER SAMARTCONFIG MODE` message is displayed, which indicates that the device has entered the configuration mode.  
   > Note: Users can enter the configuration mode by pressing the `boot` button on the development board for 1 to 5 seconds at any time. If the development board has not received the SSID and the password in 60 seconds, then the `一键配网 (Auto Configuration)` is considered a timeout. At this point, the device will switch to the `热点配网 (Hotspot Configuration)` mode automatically.

    <img src="../../documents/_static/example/alink_demo/esp32_devkit_c.png" width = "300" alt="esp32_devkit_c" align=center />
    <img src="../../documents/_static/example/alink_demo/start_config_log.png" width = "300" alt="start_config_log" align=center />

6. Select a router on your app and enter the password, then click `搜索设备 (Search Devices)` to start the configuration.

    <img src="../../documents/_static/example/alink_demo/start_configure.png" width = "300" alt="start_configure" align=center />
    <img src="../../documents/_static/example/alink_demo/configuring.png" width = "300" alt="configuring" align=center />

7. After the device has joined the network successfully, a `等待激活 (Waiting for Activation)` screen is displayed. Press the `boot` button on the development board and trigger the activation command to activate the device.

    <img src="../../documents/_static/example/alink_demo/active_device.png" width = "300" alt="active_device" align=center />

8. After the device has been activated successfully, a `开始使用 (Begin to Use)` screen is displayed. Here, you can modify the device name and assign to this device the username you prefer. Then, click `开启设备 (Open Device)` to display the `控制设备 (Device Control)` screen, where the user can switch on/off and control the device.

    <img src="../../documents/_static/example/alink_demo/enable_device.png" width = "300" alt="enable_device" align=center />
    <img src="../../documents/_static/example/alink_demo/control_panel.png" width = "300" alt="control_panel" align=center />

Note：If use smart plug demo, APP choose the device named `ESP-PLUG-DEMO-V5`, which can be found in category named `插座排插（plug）`.

## 4. How to complete the Hotspot Configuration

1. When the `一键配网 (Auto Configuration)` is failed (Timeout), `热点配网 (Hotspot Configuration)` can also be used to complete the configuration.

    <img src="../../documents/_static/example/alink_demo/config_fail.png" width = "300" alt="config_fail" align=center />

2. In the `热点配网 (Hotspot Configuration)` mode, your phone will create a hotspot, which has "aha" as an SSID and "12345678" as a password. The device will continue searching for this hotspot, and establish a connection with the hotspot after finding it.

    <img src="../../documents/_static/example/alink_demo/device_connect_hotpot.png" width = "300" alt="device_connect_hotpot" align=center />
    <img src="../../documents/_static/example/alink_demo/connect_hotpot_success.png" width = "300" alt="connect_hotpot_success" align=center />

3. At this moment, the app will prompt you to select the router that will be used subsequently, and enter the corresponding password. The app will send the router's SSID and password to the device through the mobile phone's hotspot.

    <img src="../../documents/_static/example/alink_demo/select_ap.png" width = "300" alt="select_ap" align=center />
    <img src="../../documents/_static/example/alink_demo/enter_passwort.png" width = "300" alt="enter_passwort" align=center />

4. After successfully receiving the SSID and password, the device disconnects from the hotspot of the mobile phone and tries to connect to the designated router. The subsequent process is consistent with the `一键配网 (Auto Configuration)` mode. That is, the device connects to the router first, then to the server, and finally triggers the activation command to complete the configuration.

    <img src="../../documents/_static/example/alink_demo/configuring_hotpot.png" width = "300" alt="configuring_hotpot" align=center />
    <img src="../../documents/_static/example/alink_demo/active_device.png" width = "300" alt="active_device" align=center />


