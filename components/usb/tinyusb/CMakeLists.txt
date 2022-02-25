idf_build_get_property(target IDF_TARGET)

set(srcs)
set(includes_public)
set(includes_private)
set(compile_options)

if(CONFIG_TINYUSB)
    if(target STREQUAL "esp32s3")
        set(tusb_mcu "OPT_MCU_ESP32S3")
        set(tusb_family "esp32sx")
    elseif(target STREQUAL "esp32s2")
        set(tusb_mcu "OPT_MCU_ESP32S2")
        set(tusb_family "esp32sx")
    else()
        # CONFIG_TINYUSB dependency has been garanteed by Kconfig logic,
        # So it's not possible that cmake goes here
        message(FATAL_ERROR "TinyUSB is not support on ${target}.")
        return()
    endif()

    list(APPEND compile_options
        "-DCFG_TUSB_MCU=${tusb_mcu}"
        "-DCFG_TUSB_DEBUG=${CONFIG_TINYUSB_DEBUG_LEVEL}"
        )

    idf_component_get_property(freertos_component_dir freertos COMPONENT_DIR)

    list(APPEND includes_private
        "${IDF_PATH}/components/tinyusb/tinyusb/hw/bsp/"
        "${IDF_PATH}/components/tinyusb/tinyusb/src/"
        "${IDF_PATH}/components/tinyusb/tinyusb/src/device"
        "${IDF_PATH}/components/tinyusb/tinyusb/src/class/bth"
        #"additions/include_private"
        )

    list(APPEND includes_public
        "${IDF_PATH}/components/tinyusb/tinyusb/src/"
        "additions/include"
        "additions/include_private"
        "additions/tusb/src/lib/networking"
        "additions/tusb/src/class/dfu"
        "additions/tusb/src/class/vendor"
        # The FreeRTOS API include convention in tinyusb is different from esp-idf
        "${freertos_component_dir}/include/freertos"
        )

    list(APPEND srcs
        "${IDF_PATH}/components/tinyusb/tinyusb/src/portable/espressif/${tusb_family}/dcd_${tusb_family}.c"
        "${IDF_PATH}/components/tinyusb/tinyusb/src/class/cdc/cdc_device.c"
        "${IDF_PATH}/components/tinyusb/tinyusb/src/class/hid/hid_device.c"
        "${IDF_PATH}/components/tinyusb/tinyusb/src/class/midi/midi_device.c"
        "${IDF_PATH}/components/tinyusb/tinyusb/src/class/msc/msc_device.c"
        "${IDF_PATH}/components/tinyusb/tinyusb/src/class/vendor/vendor_device.c"
        "${IDF_PATH}/components/tinyusb/tinyusb/src/common/tusb_fifo.c"
        "${IDF_PATH}/components/tinyusb/tinyusb/src/device/usbd_control.c"
        "${IDF_PATH}/components/tinyusb/tinyusb/src/tusb.c"
        #"${IDF_PATH}/components/tinyusb/tinyusb/src/device/usbd.c"
        #"${IDF_PATH}/components/tinyusb/additions/src/descriptors_control.c"
        #"${IDF_PATH}/components/tinyusb/additions/src/tinyusb.c"
        #"${IDF_PATH}/components/tinyusb/additions/src/tusb_tasks.c"
        #"${IDF_PATH}/components/tinyusb/additions/src/usb_descriptors.c"
        "additions/src/usbd.c"
        "additions/src/descriptors_control.c"
        "additions/src/tinyusb.c"
        "additions/src/tusb_tasks.c"
        "additions/src/usb_descriptors.c"
        )

    # when no builtin class driver is enabled, an uint8_t data compared with `BUILTIN_DRIVER_COUNT` will always be false
    set_source_files_properties("tinyusb/src/device/usbd.c" PROPERTIES COMPILE_FLAGS "-Wno-type-limits")

    if(CONFIG_TINYUSB_CDCACM_ENABLED)
        list(APPEND srcs
            "additions/src/cdc.c"
            "additions/src/tusb_cdc_acm.c"
            "additions/src/tusb_console.c"
            "additions/src/vfs_tinyusb.c"
            )
    endif() # CONFIG_TINYUSB_CDCACM_ENABLED

    if(CONFIG_TINYUSB_HID_ENABLED)
      list(APPEND srcs
            "additions/src/tusb_hid.c")
    endif()

    if(CONFIG_TINYUSB_MSC_ENABLED)
      list(APPEND srcs
          "additions/src/tusb_msc.c")
    endif()

    if(CONFIG_TINYUSB_NET_ENABLED)
      list(APPEND srcs
          "additions/src/tusb_net.c"
          "additions/tusb/src/class/net/net_device.c"
          "additions/tusb/src/lib/networking/rndis_reports.c")
    endif()

    if(CONFIG_TINYUSB_BTH_ENABLED)
      list(APPEND srcs
          "additions/src/tusb_bth.c"
          "${IDF_PATH}/components/tinyusb/tinyusb/src/class/bth/bth_device.c")
    endif()

    if(CONFIG_TINYUSB_DFU_ENABLED)
      list(APPEND srcs
          "additions/src/tusb_dfu.c"
          "additions/tusb/src/class/dfu/dfu_device.c")
    endif()

    if(CONFIG_TINYUSB_WEBUSB_ENABLED)
      list(APPEND srcs
        "additions/src/tusb_webusb.c"
        "additions/tusb/src/class/vendor/vendor_device.c")
    endif()
endif() # CONFIG_TINYUSB

idf_component_register(SRCS ${srcs}
                       INCLUDE_DIRS ${includes_public}
                       PRIV_INCLUDE_DIRS ${includes_private}
                       PRIV_REQUIRES "vfs" "fatfs" "bt" "app_update"
                       )

if(CONFIG_TINYUSB)
    target_compile_options(${COMPONENT_LIB} PRIVATE ${compile_options})
endif()
