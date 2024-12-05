idf_component_register(SRC_DIRS "." "hal"
                        INCLUDE_DIRS "include"
                        REQUIRES "esp_event" "esp_timer" "driver"
                        LDFRAGMENTS linker.lf)
