set(srcs "")
set(include "")
set(priv_req "bt")

if(CONFIG_BLE_MIDI_PROFILE)
list(APPEND srcs            "src/esp_ble_midi.c")
list(APPEND include         "include")
endif()

idf_component_register(SRCS "${srcs}"
                       INCLUDE_DIRS "${include}"
                       PRIV_REQUIRES "${priv_req}")
