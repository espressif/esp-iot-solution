# Component: wifi

* At first, you have to call iot_wifi_setup to initilize wifi(only WIFI_MODE_STA is available now).
* If iot_wifi_setup is called, you can call iot_wifi_connect to connect esp32 to AP.The third argument is system tick numbers to wait and 
  the function would block until esp32 connect to AP or system tick numbers cost.
* iot_wifi_get_status can be called to get current status of esp32 station.
