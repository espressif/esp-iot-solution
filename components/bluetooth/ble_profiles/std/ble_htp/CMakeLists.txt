set(srcs "")
set(req "")
set(include "")
set(priv_includes "")
set(priv_req "ble_conn_mgr")

if(CONFIG_BLE_HEALTH_THERMOMETER_PROFILES)
list(APPEND srcs            "src/esp_htp.c")
list(APPEND include         "include")
endif()

idf_component_register(SRCS "${srcs}"
                       INCLUDE_DIRS "${include}"
                       PRIV_INCLUDE_DIRS "${priv_includes}"
                       REQUIRES "${req}"
                       PRIV_REQUIRES "${priv_req}")
