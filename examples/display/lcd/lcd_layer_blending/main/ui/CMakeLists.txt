SET(SOURCES screens/ui_ScreenRGB8888.c
    ui.c
    components/ui_comp_hook.c
    ui_helpers.c
    ui_events.c
    images/ui_img_espressif_qrcode_png_small.c)

add_library(ui ${SOURCES})
