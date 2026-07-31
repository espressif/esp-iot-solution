idf_component_register(
    SRCS
        "esp_hidd_prf_api.c"
        "hid_dev.c"
        "hid_device_le_prf.c"
    INCLUDE_DIRS
        "include"
        "$ENV{IDF_PATH}/components/bt/host/bluedroid/api/include/api"
    REQUIRES
        bt
)

target_compile_options(${COMPONENT_LIB} PRIVATE -Wno-unused-const-variable)
