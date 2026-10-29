set(srcs "")
set(include "")
set(priv_includes "")
set(req "ble_services")
set(priv_req "ble_conn_mgr" "bt")

if(CONFIG_BLE_ESL_PROFILE OR CONFIG_BLE_ESL_PROFILE_AP)
list(APPEND srcs            "src/esp_eslp_state.c"
                            "src/esp_eslp_ead.c")
list(APPEND include         "include")
list(APPEND priv_includes   "private")
idf_component_get_property(_ble_esl_bt_dir bt COMPONENT_DIR)
list(APPEND priv_includes   "${_ble_esl_bt_dir}/host/nimble/nimble/nimble/host/include")
endif()

if(CONFIG_BLE_ESL_PROFILE)
list(APPEND srcs            "src/esp_eslp_tag.c"
                            "src/esp_eslp_tag_cmd.c")
endif()

if(CONFIG_BLE_ESL_PROFILE_AP)
list(APPEND srcs            "src/esp_eslp_ap.c"
                            "src/esp_eslp_ap_lifecycle.c"
                            "src/esp_eslp_ap_cmd.c"
                            "src/esp_eslp_ap_info.c")
endif()

if(CONFIG_BLE_ESL_OTS_SUPPORT)
list(APPEND srcs            "src/esp_eslp_otp_tag.c")
endif()

if(CONFIG_BLE_ESL_PROFILE_AP)
list(APPEND srcs            "src/esp_eslp_otp_ap.c")
endif()

if(CONFIG_BLE_ESL_OTS_SUPPORT OR CONFIG_BLE_ESL_PROFILE_AP)
    list(APPEND req             "ble_otp")
endif()

idf_component_register(SRCS "${srcs}"
                       INCLUDE_DIRS "${include}"
                       PRIV_INCLUDE_DIRS "${priv_includes}"
                       REQUIRES "${req}"
                       PRIV_REQUIRES "${priv_req}")
