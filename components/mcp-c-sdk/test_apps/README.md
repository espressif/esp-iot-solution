# MCP C SDK Test Application

This directory contains unit tests for the MCP C SDK component.

## Test Coverage

The test application covers all public APIs:

### MCP Transport Manager API (`esp_mcp.h`)
- `esp_mcp_mgr_init()` / `esp_mcp_mgr_deinit()`
- `esp_mcp_mgr_start()` / `esp_mcp_mgr_stop()`
- `esp_mcp_mgr_register_endpoint()` / `esp_mcp_mgr_unregister_endpoint()`
- `esp_mcp_mgr_req_handle()` - Request handling with JSON-RPC messages
- `esp_mcp_mgr_req_destroy_response()` - Response buffer cleanup
- Error handling (NULL parameters, invalid handles, invalid transport)
- Integration with `esp_mcp_t`

### MCP Engine API (`esp_mcp_engine.h`)
- `esp_mcp_create()` / `esp_mcp_destroy()`
- `esp_mcp_add_tool()` / `esp_mcp_remove_tool()`
- Error handling (NULL parameters, invalid states)

### Tool API (`esp_mcp_tool.h`)
- `esp_mcp_tool_create()` / `esp_mcp_tool_destroy()`
- `esp_mcp_tool_add_property()` / `esp_mcp_tool_remove_property()`
- Error handling (NULL parameters)

### Property API (`esp_mcp_property.h`)
- All property creation functions (bool, int, float, string, array, object, range)
- `esp_mcp_property_destroy()`
- Error handling (NULL parameters)

### Value API (`esp_mcp_data.h`)
- All value creation functions (bool, int, float, string)
- `esp_mcp_value_destroy()`
- Error handling (NULL parameters, invalid types)

### Integration Tests
- Server + Tool integration
- Tool + Property integration
- Multiple properties per tool
- MCP Transport Manager + Server integration
- Full workflow (create server, add tool with properties, register endpoint, handle requests, cleanup)
- MCP protocol message handling (initialize, tools/list, tools/call)

### Thread Safety Tests
- Concurrent tool add/remove operations
- Multi-threaded access to server

### Memory Leak Tests
- Server lifecycle
- Tool lifecycle
- Property lifecycle
- Value lifecycle
- Full workflow

## Building and Running

### Prerequisites
- ESP-IDF v5.0 or later
- Unity test framework (included in ESP-IDF)

### Build
```bash
cd components/mcp-c-sdk/test_apps
idf.py build
```

### Run All Tests
```bash
idf.py -p <PORT> flash monitor
```

### Run Specific Test Groups
```bash
# Run only server tests
idf.py -p <PORT> flash monitor --filter "server"

# Run only tool tests
idf.py -p <PORT> flash monitor --filter "tool"

# Run thread safety tests
idf.py -p <PORT> flash monitor --filter "thread_safety"

# Run memory leak tests
idf.py -p <PORT> flash monitor --filter "memory"
```

## Test Structure

Tests are organized by API category:
- `[mcp]` - MCP Transport Manager API tests
- `[server]` - Server API tests
- `[tool]` - Tool API tests
- `[property]` - Property API tests
- `[value]` - Value API tests
- `[integration]` - Integration tests combining multiple APIs
- `[thread_safety]` - Thread safety tests
- `[memory]` - Memory leak tests

## Notes

- All tests use Unity test framework
- Tests are designed to be run on actual hardware or QEMU
- Thread safety tests use FreeRTOS tasks to simulate concurrent access
- Memory leak tests verify proper resource cleanup
- MCP Transport Manager tests use a mock transport implementation (no actual HTTP server required)
- Integration tests verify end-to-end workflows including MCP protocol message handling
- Total test cases: 53+

