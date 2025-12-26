Model Context Protocol (MCP)
==============================
:link_to_translation:`en:[English]`

==================  ===========  ===============  ===============  =============
Supported chips     ESP32        ESP32-C2         ESP32-C3         ESP32-S3
==================  ===========  ===============  ===============  =============

MCP component provides a simplified API interface for accessing common MCP functions. It supports HTTP and other common scenarios. 

Examples
--------------

1. MCP Server Example: :example:`mcp/mcp_server`. Runs an MCP server on ESP32 over HTTP, exposing JSON-RPC tools for simple device controls (volume, brightness, theme, HSV/RGB color).

API Reference
-----------------

.. include-build-file:: inc/esp_mcp_mgr.inc
