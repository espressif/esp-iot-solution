set(main_srcs
    "main.cpp"
    "airmouse_ble.c"
    "airmouse_bmi270.cpp"
    "airmouse_btn.c"
    "airmouse_function.cpp"
    "airmouse_gyro_bias.cpp"
    "airmouse_gesture.cpp"
    "airmouse_gesture_inference.cpp"
    "airmouse_hid.cpp"
    "airmouse_runtime_config.c"
    "airmouse_server.c"
    "airmouse_config.c"
)

list(APPEND main_srcs "airmouse_pose_mapping_quaternion.cpp")

set(main_requires
    bt
    nvs_flash
    console
    esp_timer
    bmi270_sensor
    bmm350_sensor
    imu_quaternion
    imu_gesture
    model_inference
    BLE_multiple_HID
    esp_http_server
    esp_wifi
    esp_event
    esp_netif
)

if(CONFIG_IDF_TARGET_ESP32P4)
    list(APPEND main_requires esp_hosted)
endif()

if(CONFIG_AIRMOUSE_ENABLE_BMM350)
    list(APPEND main_srcs "airmouse_bmm350.cpp")
endif()

idf_component_register(
    SRCS ${main_srcs}
    REQUIRES ${main_requires}
    EMBED_FILES
        "web/index.html"
        "web/app.js"
        "web/style.css"
    INCLUDE_DIRS "include" "."
)

target_compile_options(${COMPONENT_LIB} PRIVATE -Wno-unused-const-variable)
