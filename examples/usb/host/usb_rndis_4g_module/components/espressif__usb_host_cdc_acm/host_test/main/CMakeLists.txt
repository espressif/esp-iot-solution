idf_component_register(SRC_DIRS .
                        REQUIRES cmock usb
                        INCLUDE_DIRS .
                        PRIV_INCLUDE_DIRS "../../private_include"
                        WHOLE_ARCHIVE)

# Currently 'main' for IDF_TARGET=linux is defined in freertos component.
# Since we are using a freertos mock here, need to let Catch2 provide 'main'.
target_link_libraries(${COMPONENT_LIB} PRIVATE Catch2WithMain)
