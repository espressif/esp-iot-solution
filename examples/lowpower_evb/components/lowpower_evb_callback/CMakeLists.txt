idf_component_register(SRCS "lowpower_evb_callback.cpp"
                        INCLUDE_DIRS "." "include"
                        REQUIRES  wifi_conn tcp smart_config lowpower_framework lowpower_evb_peripherals lowpower_evb_ulp lowpower_evb_wifi)