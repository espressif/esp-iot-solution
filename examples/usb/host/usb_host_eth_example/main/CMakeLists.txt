idf_component_register(SRCS "usb_host_eth_example_main.c"
                    INCLUDE_DIRS "."
                    PRIV_REQUIRES usb protocol_examples_common nvs_flash xfer_http
                    )
