set(srcs
    "src/esp_mcp_mgr.c"
    "src/esp_mcp_engine.c"
    "src/esp_mcp_property.c"
    "src/esp_mcp_tool.c"
    "src/esp_mcp_data.c"
)

set(priv_req "esp_common" "app_update" "json" "esp_http_server")

# Conditionally compile built-in HTTP transport
if(CONFIG_MCP_TRANSPORT_HTTP)
    list(APPEND srcs "src/esp_mcp_http.c")
endif()

set(include "include")
set(req "")
set(priv_includes "src/priv_include")

idf_component_register(SRCS "${srcs}"
                       INCLUDE_DIRS "${include}"
                       PRIV_INCLUDE_DIRS "${priv_includes}"
                       REQUIRES "${req}"
                       PRIV_REQUIRES "${priv_req}")
