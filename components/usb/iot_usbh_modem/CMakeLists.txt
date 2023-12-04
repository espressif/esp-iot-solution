set(srcs "src/esp_modem.c"
        "src/esp_modem_dce_command_lib.c"
        "src/esp_modem_dce_common_commands.c"
        "src/esp_modem_dce.c"
        "src/esp_modem_usb_dte.c"
        "src/esp_modem_netif.c"
        "src/esp_modem_recov_helper.c"
        "src/usbh_modem_board.c"
        "src/usbh_modem_wifi.c")

idf_component_register(SRCS "${srcs}"
                    INCLUDE_DIRS "include"
                    PRIV_INCLUDE_DIRS "private_include"
                    REQUIRES driver esp_netif esp_wifi
                    PRIV_REQUIRES iot_usbh_cdc esp_event)

include(package_manager)
cu_pkg_define_version(${CMAKE_CURRENT_LIST_DIR})
