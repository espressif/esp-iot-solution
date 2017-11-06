# Component: smartconfig

* We can choose `ESPTOUCH` mode or `WeChat` mode.
* At first, call smartconfig_setup to initialize wifi and smartconfig.
* After smartconfig_setup, you can call smartconfig_start to start smartconfig.The argument is system tick numbers to wait and the function would block until smartconfig complete or system tick numbers cost or smartconfig_stop is called.
* This smartconfig APIs are thread-safe and will be blocked if other smartconfig tasks are still running.
* Call iot_sc_get_status() to check the current status.