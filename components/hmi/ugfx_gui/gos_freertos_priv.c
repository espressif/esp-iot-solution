/*
 * This file is subject to the terms of the GFX License. If a copy of
 * the license was not distributed with this file, you can obtain one at:
 *
 *              http://ugfx.org/license.html
 */

#include <string.h>
#include "freertos/portmacro.h"
#include "gos_freertos_priv.h"


PRIVILEGED_DATA portMUX_TYPE lockMux_priv = portMUX_INITIALIZER_UNLOCKED;
