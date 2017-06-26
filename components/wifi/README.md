# Component: wifi

* At first, you have to call wifi_setup to initilize wifi(only WIFI_MODE_STA is available now).
* If wifi_setup is called, you can call wifi_connect_start to connect esp32 to AP.The third argument is system tick numbers to wait and 
  the function would block until esp32 connect to AP or system tick numbers cost.
* wifi_get_status can be called to get current status of esp32 station.
