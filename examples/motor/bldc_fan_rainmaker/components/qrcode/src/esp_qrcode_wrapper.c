/*
 * SPDX-FileCopyrightText: 2020-2023 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdio.h>
#include <esp_err.h>

#include "qrcodegen.h"
#include "qrcode.h"

int esp_qrcode_get_size(esp_qrcode_handle_t qrcode)
{
    return qrcodegen_getSize(qrcode);
}

bool esp_qrcode_get_module(esp_qrcode_handle_t qrcode, int x, int y)
{
    return qrcodegen_getModule(qrcode, x, y);
}
