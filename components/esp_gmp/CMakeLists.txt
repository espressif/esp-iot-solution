# Conditional SRCS/INCLUDE_DIRS may use CONFIG_*.
# REQUIRES cannot rely on list(APPEND) of CONFIG-gated deps: ESP-IDF early
# expansion only sees the literal REQUIRES set at idf_component_register().
# Use idf_component_optional_requires() after register for trimable deps.

set(GMP_SRCS
    "src/esp_gmp.c"
    "src/esp_gmp_frame.c"
    "src/esp_gmp_crc.c"
    "src/esp_gmp_flux.c"
)

set(GMP_INCLUDE_DIRS
    "include"
    "common/include"
)

set(GMP_PRIV_INCLUDE_DIRS "")

# Shared SHA-256 helper (used by OTA and/or FT)
if(CONFIG_ESP_GMP_PROFILE_OTA OR CONFIG_ESP_GMP_PROFILE_FILE_TRANSFER)
    list(APPEND GMP_SRCS "common/src/esp_gmp_sha256.c")
endif()

# OS profile
if(CONFIG_ESP_GMP_PROFILE_OS)
    list(APPEND GMP_SRCS "profiles/os/src/esp_gmp_os.c")
    list(APPEND GMP_INCLUDE_DIRS "profiles/os/include")
endif()

# OTA profile: proto always with either role; device/host independently
if(CONFIG_ESP_GMP_PROFILE_OTA)
    list(APPEND GMP_SRCS "profiles/ota/src/esp_gmp_ota_proto.c")
    list(APPEND GMP_INCLUDE_DIRS "profiles/ota/include")

    if(CONFIG_ESP_GMP_OTA_DEVICE)
        list(APPEND GMP_SRCS "profiles/ota/src/esp_gmp_ota_device.c")
    endif()

    if(CONFIG_ESP_GMP_OTA_HOST)
        list(APPEND GMP_SRCS "profiles/ota/src/esp_gmp_ota_host.c")
    endif()
endif()

# File Transfer profile
if(CONFIG_ESP_GMP_PROFILE_FILE_TRANSFER)
    list(APPEND GMP_SRCS
        "profiles/file_transfer/src/esp_file_transfer.c"
        "profiles/file_transfer/src/esp_file_transfer_secondary.c"
        "profiles/file_transfer/src/esp_file_transfer_sender.c"
        "profiles/file_transfer/src/esp_file_transfer_receiver.c"
        "profiles/file_transfer/src/esp_file_transfer_protocol.c"
        "profiles/file_transfer/src/esp_file_transfer_gmp.c"
        "profiles/file_transfer/src/esp_file_transfer_data.c"
        "profiles/file_transfer/src/esp_file_transfer_data_pipe.c"
        "profiles/file_transfer/src/esp_file_transfer_fs.c"
        "profiles/file_transfer/src/esp_file_transfer_hash.c"
    )
    list(APPEND GMP_INCLUDE_DIRS "profiles/file_transfer/include")
    list(APPEND GMP_PRIV_INCLUDE_DIRS "profiles/file_transfer/private_include")
endif()

idf_component_register(
    SRCS ${GMP_SRCS}
    INCLUDE_DIRS ${GMP_INCLUDE_DIRS}
    PRIV_INCLUDE_DIRS ${GMP_PRIV_INCLUDE_DIRS}
    REQUIRES esp_flux freertos esp_timer
)

if(CONFIG_ESP_GMP_PROFILE_OTA OR CONFIG_ESP_GMP_PROFILE_FILE_TRANSFER)
    idf_component_optional_requires(PUBLIC mbedtls)
endif()

if(CONFIG_ESP_GMP_OTA_DEVICE)
    idf_component_optional_requires(PUBLIC app_update esp_ringbuf)
endif()

if(CONFIG_ESP_GMP_PROFILE_FILE_TRANSFER)
    idf_component_optional_requires(PRIVATE fatfs)
endif()
