idf_component_register(
    SRCS "app_wifi.c" "app_http_server.c"
    INCLUDE_DIRS "include"
    REQUIRES "esp_http_server" "esp_wifi" "esp_event" "nvs_flash" "vfs"
    EMBED_FILES "./spiffs/favicon.ico"
                "./spiffs/upload.html"
                "./spiffs/file_list_1.html"
                "./spiffs/file_list_2.html"
                "./spiffs/settings.html"
                "./spiffs/styles.css"
)
