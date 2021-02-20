
set(TOUCH_DIR "xpt2046" "ft5x06" "ns2016")

idf_component_register(SRC_DIRS "." "${TOUCH_DIR}" "calibration" "calibration/basic_painter" "calibration/basic_painter/fonts"
                        INCLUDE_DIRS "." "${TOUCH_DIR}" "calibration/basic_painter" "calibration/basic_painter/fonts"
                        PRIV_INCLUDE_DIRS "calibration"
                        PRIV_REQUIRES bus nvs_flash
                        REQUIRES screen)