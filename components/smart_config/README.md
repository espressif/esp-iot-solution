# Component: smartconfig

* At first, call smartconfig_setup to initialize wifi and smartconfig.
* After smartconfig_setup, you can call smartconfig_start to start smartconfig.The argument is system tick numbers to wait and the function would block until smartconfig complete or system tick numbers cost or smartconfig_stop is called.