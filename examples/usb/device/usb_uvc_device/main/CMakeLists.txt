set(srcs  "usb_uvc_device.c" "usb_cam1.c")
set(embed_files "800_572.jpeg")
if(CONFIG_UVC_SUPPORT_TWO_CAM)
    list(APPEND srcs "usb_cam2.c")
endif()

idf_component_register(SRCS  "${srcs}"
                    INCLUDE_DIRS "."
                    EMBED_FILES "${embed_files}")
