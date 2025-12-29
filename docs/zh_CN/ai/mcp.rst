模型上下文协议 (Model Context Protocol, MCP)
=============================================
:link_to_translation:`en:[English]`

==================  ===========  ===============  ===============  =============  =============
支持的芯片            ESP32        ESP32-C2          ESP32-C3         ESP32-C6       ESP32-S3
==================  ===========  ===============  ===============  =============  =============

MCP 组件为访问常用 MCP 功能提供了简化的 API 接口。它支持 HTTP 等常见场景。 

示例
--------------

1. MCP 服务示例: :example:`mcp/mcp_server`. 在 ESP32 系列芯片上运行 MCP 服务器，通过 HTTP 暴露 JSON-RPC 工具，用于简单的设备控制（音量、亮度、主题、HSV/RGB 颜色）。

2. MCP 客户端示例: :example:`mcp/mcp_client`. 在 ESP32 系列芯片上运行 MCP 客户端，通过 HTTP 访问远程 MCP 服务器。

API 参考
-----------------

.. include-build-file:: inc/esp_mcp_mgr.inc
