# ChangeLog

## [v1.0.0]

### Added
- **MCP Client Support**: Added complete HTTP client transport implementation for outbound MCP requests
  - New transport: `esp_mcp_transport_http_client` for client-side MCP communication:
    - Automatic URL construction from base URL and endpoint name
    - Support for chunked and non-chunked HTTP responses
    - Automatic response buffer management and cleanup
    - Error handling for network failures and timeouts
  - New API functions for building and sending MCP requests:
    - `esp_mcp_mgr_post_info_init()`: Send initialize request to remote MCP server
    - `esp_mcp_mgr_post_tools_list()`: Send tools/list request with optional cursor-based pagination
    - `esp_mcp_mgr_post_tools_call()`: Send tools/call request to execute remote tools
  - New low-level API: `esp_mcp_mgr_perform_handle()` for sending custom JSON-RPC requests
  - Asynchronous response handling with callback mechanism via `esp_mcp_mgr_resp_cb_t`
  - Example client application (`examples/mcp/mcp_client`) demonstrating complete client usage
- Added `esp_mcp_mgr_req_t` structure for building MCP requests with union-based parameters for different request types
- Added `esp_mcp_mgr_resp_cb_t` callback type for handling asynchronous responses from outbound requests
- Added deprecated alias `esp_mcp_transport_http` for backward compatibility (maps to `esp_mcp_transport_http_server`)
- Added transport layer separation: `esp_mcp_transport_http_server` and `esp_mcp_transport_http_client` as distinct transports

### Changed
- Changed transport structure: added optional `request` callback to `esp_mcp_transport_t` for client-side outbound requests
- Separated HTTP transports: split `esp_mcp_transport_http` into `esp_mcp_transport_http_server` and `esp_mcp_transport_http_client`

### Improved
- Improved HTTP server transport layer error handling and resource management
  - Added content length validation (max 1MB) and timeout handling (max 10 consecutive timeouts)
  - Improved error reporting for invalid requests and receive timeouts
  - Changed `sprintf` to `snprintf` for safer string formatting
- Improved architecture clarity: documented 3-layer architecture (Manager/Router, Transport, Engine) in README
- Improved code organization: separated HTTP server and client transports into distinct implementations

### Fixed
- Fixed missing `SLIST_REMOVE` call in `esp_mcp_mgr_delete()` when cleaning up pending callbacks

### Deprecated
- **Transport API**: The `esp_mcp_transport_http` symbol is deprecated and will be removed in a future release
  - Use `esp_mcp_transport_http_server` for server mode
  - Use `esp_mcp_transport_http_client` for client mode
  - The deprecated alias currently maps to `esp_mcp_transport_http_server` for backward compatibility

## v0.1.0

This is the first release version for [Model Context Protocol](https://modelcontextprotocol.io/specification/versioning) component in Espressif Component Registry, more detailed descriptions about the project, please refer to [User_Guide](https://docs.espressif.com/projects/esp-iot-solution/en/latest/mcp/mcp-c-sdk.html).

Features:
- JSON-RPC 2.0 Communication: Request/response communication based on JSON-RPC 2.0 standard
- Tool Registration: Register and call tools with structured parameters
- Transport Support: Supports HTTP server communication methods
