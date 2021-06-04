
idf_component_register(SRCS app_httpd.c app_wifi.c 
                    INCLUDE_DIRS "." "include"
                    PRIV_REQUIRES nvs_flash lwip nghttp esp_http_server
                    EMBED_FILES
                    "www/index_uvc.html.gz")
