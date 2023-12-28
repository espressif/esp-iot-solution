set(srcs "foc_knob.c"
        "foc_knob_default.c"
        )

set(includes
        "include"
    )

idf_component_register(SRCS ${srcs}
                        INCLUDE_DIRS "${includes}"
                        REQUIRES driver esp_timer)
