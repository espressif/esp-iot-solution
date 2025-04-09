idf_component_register(SRCS "usb_host_rndis.c"
                       INCLUDE_DIRS "include"
                       REQUIRES esp_netif esp_ringbuf
                       )
