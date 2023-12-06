idf_component_register(SRC_DIRS "."
                       INCLUDE_DIRS "."
                       REQUIRES unity test_utils openai protocol_examples_common esp_netif nvs_flash esp_wifi driver
                       EMBED_FILES "../audio/turn_on_tv_en.mp3" "../audio/introduce_espressif.mp3"
                       )

add_definitions(-DCI_OPENAI_KEY="$ENV{CI_OPENAI_KEY}")
