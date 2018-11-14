
# componet standalone mode
if(NOT CONFIG_IOT_SOLUTION_EMBED)
    set(COMPONENT_SRCS "espmqtt/mqtt_client.c"
                        "espmqtt/lib/mqtt_msg.c"
                        "espmqtt/lib/mqtt_outbox.c"
                        "espmqtt/lib/platform_esp32_idf.c"
                        "espmqtt/lib/transport_ssl.c"
                        "espmqtt/lib/transport_tcp.c"
                        "espmqtt/lib/transport_ws.c"
                        "espmqtt/lib/transport.c")

    set(COMPONENT_ADD_INCLUDEDIRS "include espmqtt/lib/include")
else()
    if(CONFIG_IOT_MQTT_ENABLE)
        set(COMPONENT_SRCS "espmqtt/mqtt_client.c"
                            "espmqtt/lib/mqtt_msg.c"
                            "espmqtt/lib/mqtt_outbox.c"
                            "espmqtt/lib/platform_esp32_idf.c"
                            "espmqtt/lib/transport_ssl.c"
                            "espmqtt/lib/transport_tcp.c"
                            "espmqtt/lib/transport_ws.c"
                            "espmqtt/lib/transport.c")

        set(COMPONENT_ADD_INCLUDEDIRS "include espmqtt/lib/include")
    else()
        set(COMPONENT_SRCS "")
        set(COMPONENT_ADD_INCLUDEDIRS "")
        message(STATUS "Building empty mqtt component due to configuration")
    endif()
endif()

# requirements can't depend on config
set(COMPONENT_REQUIRES lwip nghttp mbedtls)

register_component()
