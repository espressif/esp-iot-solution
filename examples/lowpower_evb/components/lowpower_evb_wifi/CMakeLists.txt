idf_component_register(SRCS "lowpower_evb_wifi.cpp"
                        INCLUDE_DIRS "." "include"
                        REQUIRES  lowpower_framework smart_config button param)