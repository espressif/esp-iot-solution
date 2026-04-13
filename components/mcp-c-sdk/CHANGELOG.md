# ChangeLog

## [v2.0.0] - 2026-04-13

Targets **MCP protocol `2025-11-25`** as the default negotiated surface (with **legacy `2024-11-05`** transport still selectable via Kconfig where applicable). This release aligns the SDK with **Streamable HTTP** (JSON-RPC over HTTP + SSE), optional **OAuth** discovery metadata, richer **auth** options, and the **Prompts / Resources / Completions / Tasks** capability set.

### Added

#### Engine, tools, and capability modules

- **Prompts**: Register static or dynamic prompts; engine handles `prompts/list` and `prompts/get` with render callbacks (`esp_mcp_prompt.h`, `esp_mcp_prompt.c`).
- **Resources**: Register resources with optional dynamic `read` callbacks; engine handles `resources/list`, `resources/templates/list`, and `resources/read` (`esp_mcp_resource.h`, `esp_mcp_resource.c`).
- **Completions**: Optional completion provider wired to `completion/complete` (`esp_mcp_completion.h`, `esp_mcp_completion.c`).
- **Tasks (experimental)**: Internal task store and lifecycle helpers (`esp_mcp_task.c` / `esp_mcp_task_priv.h`, not public API) supporting async `tools/call` flows and JSON-RPC **`tasks/list`**, **`tasks/get`**, **`tasks/result`**, **`tasks/cancel`** in the engine.
- **Tools**: Expanded tool registration and execution paths to match updated MCP method behavior (including task-augmented `tools/call` where enabled).

#### HTTP server transport (`esp_mcp_http_server.c`)

- **Streamable HTTP**: MCP primary JSON-RPC on **`POST`**; **SSE** stream on **`GET`** with `Accept: text/event-stream` for server-initiated JSON-RPC notifications and downstream traffic.
- **Sessions**: **`MCP-Session-Id`** header support, session registry, TTL / idle behavior (Kconfig), and session termination (**`DELETE`**) where configured.
- **Protocol negotiation**: **`MCP-Protocol-Version`** response header and compatibility checks against client `Protocol-Version` / request headers.
- **Authentication**: Transport-level **Bearer** token checks; optional **JWT** verification skeleton (claims, audience, issuer, time skew, optional **JWKS** / inline key material, optional **signature** path); **`WWW-Authenticate`** and scope-insufficient paths for OAuth-style clients.
- **OAuth metadata**: HTTP handlers for **`.well-known/oauth-protected-resource`** and **`.well-known/oauth-authorization-server`** when enabled.
- **CORS**: Optional staging of **`Access-Control-Allow-Origin`** and related headers for browser clients (with safe pointer lifetime handling vs `esp_http_server` header storage).
- **Operational responses**: Structured HTTP status usage (e.g. **201/202** session creation, **406** unacceptable SSE, **503** overload), **`ping`** health, and clearer error bodies for JSON-RPC errors where applicable.
- **SSE implementation**: Async client tasks, event IDs / sequencing, optional resume hints, queueing, and backpressure-related safeguards (see Kconfig for buffer sizes and task stack).

#### HTTP client transport (`esp_mcp_http_client.c`)

- **Streamable client**: Outbound **`POST`** for JSON-RPC; inbound **SSE** parsing for default `message` events and dispatch into the manager.
- **Session & auth**: Track **`MCP-Session-Id`** from responses; apply **Authorization** headers; retry paths for **401/403** where implemented; align **`HTTP_EVENT_ON_FINISH`** handling so pending manager callbacks are not left unresolved on error statuses.

#### Manager (`esp_mcp_mgr.c`)

- **Request context**: Per-request session / routing context for tools, tasks, and notifications.
- **Pending JSON-RPC**: Queue and completion of outbound/inbound requests so `tools/call`, `tasks/*`, and SSE-delivered messages resolve correctly.
- **Integration**: Coordinates engine, transports, and task objects for **initialize**, **notifications**, **tools/***, **resources/***, **prompts/***, **completion/***, **tasks/***, and **logging** where exposed.

#### Tests (`components/mcp-c-sdk/test_apps`)

- **On-target**: Large expansion of `test_mcp_c_sdk.c` covering initialization, tools (list/call/pagination), resources, prompts, completions, tasks, auth gates, SSE behavior, and error codes.
- **Host pytest**: `pytest_mcp.py` updates; new **`pytest_mcp_http_integration.py`** for HTTP-level checks against a running server.
- **Runner**: **`run_test_apps.sh`** to drive builds and tests with multiple **`sdkconfig.ci.*`** profiles.
- **CI profiles**: Dedicated **`sdkconfig.ci.*`** fragments for combinations such as **SSE on/off**, **auth on/off**, **JWT claims / signature**, **OAuth metadata on/off**, **protocol default 2024**, **session TTL**, **tool list/call limits**, missing HTTP client/server components, etc.
- **Repository wiring**: Root **`pytest.ini`** and **`tools/ci/executable-list.txt`** updates so CI can discover and run the new scripts.

#### Documentation (`docs/`)

- **User guides (EN & ZH)**: New topic pages under `docs/en/mcp/` and `docs/zh_CN/mcp/`:
  - `core_and_manager.rst` — engine vs manager responsibilities
  - `tooling_and_data.rst` — tools and structured data
  - `prompt_resource_completion.rst` — prompts, resources, completions
- **Overview**: `docs/en/ai/mcp.rst` and `docs/zh_CN/ai/mcp.rst` updated to link into the split structure.
- **API reference**: `docs/Doxyfile` **INPUT** extended with **`esp_mcp_prompt.h`**, **`esp_mcp_resource.h`**, **`esp_mcp_completion.h`** (in addition to existing engine/mgr/tool/data/property headers).

#### Examples (`examples/mcp/`)

- **`mcp_client`**: Updated **`main/Kconfig`**, **`sdkconfig.defaults`**, **`partitions.csv`**, **`README.md`**, and **`mcp_client.c`** (initialize ordering, tools/list/call, optional resources/prompts demos, pagination); added **`README_CN.md`**.
- **`mcp_server`**: Extended **`mcp_server.c`** and server **`Kconfig`**; **`sdkconfig.defaults`** and **`partitions.csv`**; expanded **`README.md`** and added **`README_CN.md`**.
- **CI matrix**: **`examples/mcp/mcp_server/ci/sdkconfig.matrix/*.defaults`** — many named presets (baseline SSE/auth, JWT variants, JWKS valid/invalid, metadata off, TTL short, health endpoint unavailable, GET **405**, server-only mode, etc.).
- **Scripts**: **`test_kconfig_matrix.sh`** (matrix validation), **`test_sse_e2e.sh`** (SSE end-to-end), **`gen_es256_test_materials.sh`** (JWT test key material for CI).

### Changed

- **HTTP server & client**: Major internal redesign of **`esp_mcp_http_server.c`** and **`esp_mcp_http_client.c`** for Streamable HTTP, session lifecycle, SSE, and auth—file size and behavior differ substantially from v1.0.x line-oriented server-only usage.
- **Engine**: **`esp_mcp_engine.c`** grown to implement the full method surface (including tasks and streaming-related notifications) and stricter protocol checks.
- **Manager**: **`esp_mcp_mgr.c`** expanded for session-aware routing, pending RPC bookkeeping, and transport callbacks; callers should re-validate threading and callback assumptions.

### Upgrade / migration

- Re-test any custom **HTTP** integration: **SSE**, **`MCP-Session-Id`**, and **auth** headers change observable wire behavior.
- If you relied only on **simple POST JSON-RPC** without sessions or SSE, enable or mock the corresponding Kconfig options and verify server logs and client sequencing (**`initialize`** must complete before **`tools/list`** / **`tools/call`** in strict clients).
- Run the updated **`test_apps`** and, where possible, **`pytest_mcp_http_integration`** against your deployment before production rollout.

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
