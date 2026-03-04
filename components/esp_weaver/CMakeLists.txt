set(srcs
    "src/esp_weaver.c"
    "src/esp_weaver_local_ctrl.c"
)

set(priv_req "esp_common" "esp_local_ctrl" "protocomm" "mdns" "json_generator" "json_parser" "esp_wifi" "esp_app_format" "nvs_flash")

set(include "include")
set(priv_includes "src/priv_include")

idf_component_register(SRCS "${srcs}"
                       INCLUDE_DIRS "${include}"
                       PRIV_INCLUDE_DIRS "${priv_includes}"
                       PRIV_REQUIRES "${priv_req}")
