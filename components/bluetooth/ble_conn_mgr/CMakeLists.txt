set(srcs "")
set(req "")
set(include "")
set(priv_includes "")
set(priv_req "")

if(CONFIG_BT_NIMBLE_ENABLED)
    list(APPEND srcs     "src/esp_nimble.c")
elseif(CONFIG_BT_BLUEDROID_ENABLED)
    list(APPEND srcs     "src/esp_bluedroid.c")
    message(FATAL_ERROR "Bluedroid support is not implemented yet for BLE connect management")
endif()
list(APPEND include "include")
list(APPEND priv_includes   "src/priv_includes")

list(APPEND req "bt")

idf_component_register(SRCS "${srcs}"
                       INCLUDE_DIRS "${include}"
                       PRIV_INCLUDE_DIRS "${priv_includes}"
                       REQUIRES "${req}"
                       PRIV_REQUIRES "${priv_req}")

if(CONFIG_BT_ENABLED)
include(package_manager)
cu_pkg_define_version(${CMAKE_CURRENT_LIST_DIR})
endif()
