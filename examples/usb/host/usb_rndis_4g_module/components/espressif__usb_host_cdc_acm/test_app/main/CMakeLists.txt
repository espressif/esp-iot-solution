idf_component_register(SRC_DIRS .
                       INCLUDE_DIRS .
                       REQUIRES usb_host_cdc_acm unity esp_tinyusb
                       WHOLE_ARCHIVE)

# So we have access to private_include:
target_include_directories(${COMPONENT_LIB} PRIVATE "../../")
