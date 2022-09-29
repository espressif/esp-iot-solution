idf_component_register(SRC_DIRS "."
                       INCLUDE_DIRS "include"
                       PRIV_INCLUDE_DIRS "${IDF_PATH}/components/usb/private_include"
                       REQUIRES usb)
