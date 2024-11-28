/*
 * SPDX-FileCopyrightText: 2023-2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

#include "esp_platform.h"
#include "BLDCMotor.h"
#include "esp_hal_bldc_3pwm.h"
#include "esp_hal_bldc_6pwm.h"
#include "communication/SimpleFOCDebug.h"
#include "communication/Commander.h"
#include "sensors/GenericSensor.h"
#include "current_sense/GenericCurrentSense.h"
#include "current_sense/LowsideCurrentSense.h"
#include "../../angle_sensor/as5600.h"
#include "../../angle_sensor/mt6701.h"
#include "../../angle_sensor/as5048a.h"
