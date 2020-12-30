
set(TOUCH_DIR "xpt2046" "ft5x06" "ns2016")

idf_component_register(SRC_DIRS "." "${TOUCH_DIR}" "calibration"
                        INCLUDE_DIRS "." "${TOUCH_DIR}"
                        PRIV_INCLUDE_DIRS "calibration"
                        PRIV_REQUIRES bus
                        REQUIRES lcd_paint)