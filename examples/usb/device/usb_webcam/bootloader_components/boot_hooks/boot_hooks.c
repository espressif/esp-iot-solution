/*
 * SPDX-FileCopyrightText: 2023 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include "esp_log.h"
#include "soc/rtc_cntl_struct.h"
#include "soc/usb_serial_jtag_reg.h"

/* Function used to tell the linker to include this file
 * with all its symbols.
 */
void bootloader_hooks_include(void)
{
}

void bootloader_before_init(void)
{
    // Disable D+ pullup, to prevent the USB host from retrieving USB-Serial-JTAG's descriptor.
    SET_PERI_REG_MASK(USB_SERIAL_JTAG_CONF0_REG, USB_SERIAL_JTAG_PAD_PULL_OVERRIDE);
    CLEAR_PERI_REG_MASK(USB_SERIAL_JTAG_CONF0_REG, USB_SERIAL_JTAG_DP_PULLUP);
    CLEAR_PERI_REG_MASK(USB_SERIAL_JTAG_CONF0_REG, USB_SERIAL_JTAG_USB_PAD_ENABLE);
}
