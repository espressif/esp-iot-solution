Model Context Protocol (MCP)
==============================
:link_to_translation:`en:[English]`

==================  ===========  ===============  ===============  =============  =============
Supported chips     ESP32        ESP32-C2         ESP32-C3         ESP32-C6       ESP32-S3
==================  ===========  ===============  ===============  =============  =============

MCP component provides a simplified API interface for accessing common MCP functions. It supports HTTP and other common scenarios. 

Examples
--------------

1. MCP Server Example: :example:`mcp/mcp_server`. Runs an MCP server on ESP32 over HTTP, exposing JSON-RPC tools for simple device controls (volume, brightness, theme, HSV/RGB color).

2. MCP Client Example: :example:`mcp/mcp_client`. Runs an MCP client on ESP32 over HTTP, accessing a remote MCP server.

API Reference
-----------------

To improve readability, MCP APIs are grouped by capability:

API Module Quick Guide
^^^^^^^^^^^^^^^^^^^^^^^^^

.. list-table::
   :header-rows: 1
   :widths: 28 72

   * - Module
     - Purpose
   * - Core and Manager
     - MCP lifecycle, request dispatch, and transport manager/session handling.
   * - Tooling and Data
     - Tool definition/execution APIs and shared property/value data structures.
   * - Prompt/Resource/Completion
     - Prompt and resource providers, plus completion provider callbacks.

.. toctree::
   :maxdepth: 1

   ../mcp/core_and_manager
   ../mcp/tooling_and_data
   ../mcp/prompt_resource_completion
