set(srcs "")
set(req "")
set(include "")
set(priv_includes "")
set(priv_req "")

if(CONFIG_BLE_ANS)
list(APPEND srcs            "ans/src/esp_ans.c")
list(APPEND include         "ans/include")
endif()

if(CONFIG_BLE_DIS)
list(APPEND srcs            "dis/src/esp_dis.c")
list(APPEND include         "dis/include")
endif()

if(CONFIG_BLE_BAS)
list(APPEND srcs            "bas/src/esp_bas.c")
list(APPEND include         "bas/include")
endif()

if(CONFIG_BLE_HRS)
list(APPEND srcs            "hrs/src/esp_hrs.c")
list(APPEND include         "hrs/include")
endif()

if(CONFIG_BLE_HTS)
list(APPEND srcs            "hts/src/esp_hts.c")
list(APPEND include         "hts/include")
endif()

if(CONFIG_BLE_TPS)
list(APPEND srcs            "tps/src/esp_tps.c")
list(APPEND include         "tps/include")
endif()

if(CONFIG_BLE_BCS)
list(APPEND srcs            "bcs/src/esp_bcs.c")
list(APPEND include         "bcs/include")
endif()

if(CONFIG_BLE_UDS)
list(APPEND srcs            "uds/src/esp_uds.c")
list(APPEND include         "uds/include")
endif()

if(CONFIG_BLE_CTS)
list(APPEND srcs            "cts/src/esp_cts.c")
list(APPEND include         "cts/include")
endif()

if(CONFIG_BLE_WSS)
list(APPEND srcs            "wss/src/esp_wss.c")
list(APPEND include         "wss/include")
endif()

if(CONFIG_BLE_MIDI)
list(APPEND srcs            "midi/src/esp_ble_midi_svc.c")
list(APPEND include         "midi/include")
endif()

list(APPEND req "ble_conn_mgr")

idf_component_register(SRCS "${srcs}"
                       INCLUDE_DIRS "${include}"
                       PRIV_INCLUDE_DIRS "${priv_includes}"
                       REQUIRES "${req}"
                       PRIV_REQUIRES "${priv_req}")
