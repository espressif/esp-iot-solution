idf_component_register(SRCS "usb_host_rndis.c" "usb_host_rndis_descriptor.c"
                       INCLUDE_DIRS "include"
                       PRIV_INCLUDE_DIRS "priv_include"
                       REQUIRES esp_netif esp_ringbuf
                       )
