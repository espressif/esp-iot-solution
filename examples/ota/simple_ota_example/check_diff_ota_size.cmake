if(NOT EXISTS "${PACKED}")
    return()
endif()

file(SIZE "${PACKED}" packed_size)
if(packed_size LESS 1024)
    message(WARNING
        " ${PACKED} is ${packed_size} bytes, shorter than 1024.\n"
        " esp_https_ota cannot finish reading the image header.\n"
        " Pack it again with --pad_https_ota:\n"
        " python3 ${SCRIPT} \\\n"
        "     -hv v3 -c xz -d ddelta -b ${BASE} -i ${NEW} --add_app_header --pad_https_ota")
endif()
