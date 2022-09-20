set(srcs "src/esp_modem.c"
        "src/esp_modem_dce_command_lib.c"
        "src/esp_modem_dce_common_commands.c"
        "src/esp_modem_dce.c"
        "src/esp_modem_usb_dte.c"
        "src/esp_modem_netif.c"
        "src/esp_modem_recov_helper.c")

set(include_dirs "include")

idf_component_register(SRCS "${srcs}"
                    INCLUDE_DIRS "${include_dirs}"
                    PRIV_INCLUDE_DIRS private_include
                    REQUIRES driver esp_usbh_cdc)
