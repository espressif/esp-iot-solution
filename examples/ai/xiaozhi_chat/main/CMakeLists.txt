set(SOURCES "esp_xiaozhi_chat_app.c" "esp_xiaozhi_chat_display.c" "app_main.c")
set(INCLUDE_DIRS ".")

idf_component_register(SRCS ${SOURCES}
                       INCLUDE_DIRS ${INCLUDE_DIRS}
                       WHOLE_ARCHIVE
                      )
