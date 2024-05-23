set(srcs
    "descriptors_control.c"
    "tinyusb.c"
    "usb_descriptors.c"
    )

if(NOT CONFIG_TINYUSB_NO_DEFAULT_TASK)
    list(APPEND srcs "tusb_tasks.c")
endif() # CONFIG_TINYUSB_NO_DEFAULT_TASK

if(CONFIG_TINYUSB_CDC_ENABLED)
    list(APPEND srcs
        "cdc.c"
        "tusb_cdc_acm.c"
        )
    if(CONFIG_VFS_SUPPORT_IO)
        list(APPEND srcs
            "tusb_console.c"
            "vfs_tinyusb.c"
            )
    endif() # CONFIG_VFS_SUPPORT_IO
endif() # CONFIG_TINYUSB_CDC_ENABLED

if(CONFIG_TINYUSB_BTH_ENABLED)
    list(APPEND srcs
        tusb_bth.c
        )
endif() # CONFIG_TINYUSB_BTH_ENABLED

if(CONFIG_TINYUSB_DFU_ENABLED)
    list (APPEND srcs
        tusb_dfu.c
        tusb_dfu_ota.c
        )
endif() # CONFIG_TINYUSB_DFU_ENABLED

if(CONFIG_TINYUSB_NET_MODE_NCM
    OR CONFIG_TINYUSB_NET_MODE_RNDIS
    OR CONFIG_TINYUSB_NET_MODE_ECM)
    list(APPEND srcs
         tinyusb_net.c
         )
endif() # CONFIG_TINYUSB_NET_MODE_NCM || CONFIG_TINYUSB_NET_MODE_ECM || CONFIG_TINYUSB_NET_MODE_RNDIS

idf_component_register(SRCS ${srcs}
                       INCLUDE_DIRS "include"
                       PRIV_INCLUDE_DIRS "include_private"
                       PRIV_REQUIRES usb
                       REQUIRES fatfs vfs bt nvs_flash app_update
                       )

# Determine whether tinyusb is fetched from component registry or from local path
idf_build_get_property(build_components BUILD_COMPONENTS)
if(tinyusb IN_LIST build_components)
    set(tinyusb_name tinyusb) # Local component
else()
    set(tinyusb_name espressif__tinyusb) # Managed component
endif()

# Pass tusb_config.h from this component to TinyUSB
idf_component_get_property(tusb_lib ${tinyusb_name} COMPONENT_LIB)
target_include_directories(${tusb_lib} PRIVATE "include")
if(CONFIG_TINYUSB_DFU_ENABLED)
    target_sources(${tusb_lib} PUBLIC "tusb_dfu.c")
endif() # CONFIG_TINYUSB_DFU_ENABLED
