# MCP C SDK Test Application

This directory contains unit tests for the MCP C SDK component.

## Test Coverage

The test application covers all public APIs:

### MCP Transport Manager API (`esp_mcp_mgr.h`)
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

### MCP 2024-11-05 Protocol Compliance Tests
- **Initialize method tests** (4 cases)
  - Verify protocolVersion field equals "2024-11-05"
  - Verify capabilities structure includes tools and experimental
  - Verify serverInfo contains name and version
  - Verify client capabilities parsing (vision sub-capabilities)
  
- **JSON-RPC 2.0 specification tests** (4 cases)
  - Verify all requests/responses contain "jsonrpc": "2.0"
  - Verify request/response ID matching
  - Verify result/error fields are mutually exclusive
  
- **tools/list complete tests** (4 cases)
  - Verify cursor parameter support for pagination
  - Verify nextCursor return when more tools available
  - Verify invalid cursor handling (returns empty list)
  - Verify pagination with multiple tools
  
- **tools/call depth tests** (9 cases: validation, range check, out-of-range, float, string, stacksize, nonexistent tool, missing param, invalid stacksize)
  - Verify parameter type validation
  - Verify parameter range checking (min/max constraints)
  - Verify stackSize parameter support
  - Verify error for nonexistent tools
  - Verify error for missing required parameters
  - Verify error for invalid stackSize values
  - Verify out-of-range parameter handling
  
- **Error handling complete tests** (6 cases)
  - Verify -32700: Parse Error (invalid JSON)
  - Verify -32600: Invalid Request (wrong jsonrpc version)
  - Verify -32601: Method not found
  - Verify -32602: Invalid params
  - Verify custom error codes (-32001 to -32003)
  - Verify error response format (code and message fields)
  
- **Response parsing tests** (4 cases)
  - Parse successful response (result field)
  - Parse error response (error field)
  - Parse application-level errors (isError field)
  - Parse invalid JSON handling
  
- **ping method tests** (1 case)
  - Verify ping response format

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

# Run MCP protocol compliance tests
idf.py -p <PORT> flash monitor --filter "protocol"

# Run initialize method tests
idf.py -p <PORT> flash monitor --filter "initialize"

# Run tools/list tests
idf.py -p <PORT> flash monitor --filter "tools/list"

# Run tools/call tests
idf.py -p <PORT> flash monitor --filter "tools/call"

# Run error handling tests
idf.py -p <PORT> flash monitor --filter "error"

# Run response parsing tests
idf.py -p <PORT> flash monitor --filter "response"
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
- `[protocol]` - MCP 2024-11-05 protocol compliance tests
  - `[initialize]` - Initialize method tests
  - `[jsonrpc]` - JSON-RPC 2.0 specification tests
  - `[tools][list]` - tools/list method tests
  - `[tools][call]` - tools/call method tests
  - `[error]` - Error handling tests
  - `[response]` - Response parsing tests
  - `[ping]` - ping method tests

## Notes

- All tests use Unity test framework
- Tests are designed to be run on actual hardware or QEMU
- Thread safety tests use FreeRTOS tasks to simulate concurrent access
- Memory leak tests verify proper resource cleanup
- MCP Transport Manager tests use a mock transport implementation (no actual HTTP server required)
- Integration tests verify end-to-end workflows including MCP protocol message handling
- MCP 2024-11-05 protocol compliance tests verify full specification adherence
- Test coverage includes JSON-RPC 2.0, all core methods, error codes, and response parsing
- Total test cases: 85 (53 API tests + 32 protocol compliance tests)

