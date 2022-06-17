idf_component_register(SRC_DIRS "."
        INCLUDE_DIRS "."
        EMBED_FILES "favicon.ico" "upload_script.html"
        PRIV_REQUIRES esp32-camera nvs_flash esp_http_server camera_example_common fatfs spiffs avi_video_process)
        