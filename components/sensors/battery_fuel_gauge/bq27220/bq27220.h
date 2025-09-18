/*
 * SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: CC0-1.0
 */
#pragma once

#include <stdint.h>
#include "esp_err.h"
#include "i2c_bus.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef union {
    struct {
        // Low byte, Low bit first
        bool DSG : 1;       /* The device is in DISCHARGE */
        bool SYSDWN : 1;    /* System down bit indicating the system should shut down */
        bool TDA : 1;       /* Terminate Discharge Alarm */
        bool BATTPRES : 1;  /* Battery Present detected */
        bool AUTH_GD : 1;   /* Detect inserted battery */
        bool OCVGD : 1;     /* Good OCV measurement taken */
        bool TCA : 1;       /* Terminate Charge Alarm */
        bool RSVD : 1;      /* Reserved */
        // High byte, Low bit first
        bool CHGINH : 1;    /* Charge inhibit */
        bool FC : 1;        /* Full-charged is detected */
        bool OTD : 1;       /* Overtemperature in discharge condition is detected */
        bool OTC : 1;       /* Overtemperature in charge condition is detected */
        bool SLEEP : 1;     /* Device is operating in SLEEP mode when set */
        bool OCVFAIL : 1;   /* Status bit indicating that the OCV reading failed due to current */
        bool OCVCOMP : 1;   /* An OCV measurement update is complete */
        bool FD : 1;        /* Full-discharge is detected */
    };
    uint16_t full;
} battery_status_t;

ESP_STATIC_ASSERT(sizeof(battery_status_t) == 2, "Incorrect structure size");

typedef struct {
    // Low byte, Low bit first
    bool CALMD : 1;         /* Calibration mode enabled */
    uint8_t SEC : 2;        /* Current security access */
    bool EDV2 : 1;          /* EDV2 threshold exceeded */
    bool VDQ : 1;           /* Indicates if Current discharge cycle is NOT qualified or qualified for an FCC updated */
    bool INITCOMP : 1;      /* gauge initialization is complete */
    bool SMTH : 1;          /* RemainingCapacity is scaled by smooth engine */
    bool BTPINT : 1;        /* BTP threshold has been crossed */
    // High byte, Low bit first
    uint8_t RSVD1 : 2;
    bool CFGUPDATE : 1;     /* Gauge is in CONFIG UPDATE mode */
    uint8_t RSVD0 : 5;
} operation_status_t;

ESP_STATIC_ASSERT(sizeof(operation_status_t) == 2, "Incorrect structure size");

typedef union {
    struct {
        // Low byte, Low bit first
        bool CCT : 1;       /* 0 = Use CC % of DesignCapacity() (default), 1 = Use CC % of FullChargeCapacity() */
        bool CSYNC : 1;     /* Sync RemainingCapacity() with FullChargeCapacity() at valid charge termination */
        bool RSVD0 : 1;
        bool EDV_CMP : 1;
        bool SC : 1;
        bool FIXED_EDV0 : 1;
        uint8_t RSVD1 : 2;
        // High byte, Low bit first
        bool FCC_LIM : 1;
        bool RSVD2 : 1;
        bool FC_FOR_VDQ : 1; /* full charge voltage for VDQ */
        bool IGNORE_SD : 1; /* ignore self-discharge */
        bool SME0 : 1;
        uint8_t RSVD3 : 3;
    };
    uint16_t full;
} gauging_config_t;
ESP_STATIC_ASSERT(sizeof(gauging_config_t) == 2, "Incorrect structure size");

/* CEDV (Compensated End-of-Discharge-Voltage) parameter set
 * These values are loaded into RAM to configure the BQ27220 gas-gauge for a specific cell chemistry.
 */
typedef struct {
    uint16_t full_charge_cap;   /* Learned Full-Charge Capacity (mAh) – actual usable capacity */
    uint16_t design_cap;        /* Design Capacity (mAh) */
    uint16_t reserve_cap;       /* Reserve Capacity (mAh) */
    uint16_t near_full;         /* Near Full threshold (mAh) */
    uint16_t self_discharge_rate; /* Self-discharge rate index (0-255) → %/day = value × 0.0025 % */
    uint16_t EDV0;              /* End-of-Discharge Voltage 0 % SOC (mV) */
    uint16_t EDV1;              /* End-of-Discharge Voltage 3 % SOC (mV) */
    uint16_t EDV2;              /* End-of-Discharge Voltage Battery-Low % SOC (mV) */
    uint16_t EMF;               /* This value is the no-load cell voltage higher than the highest cell EDV threshold computed */
    uint16_t C0;                /* This value is the no-load, capacity related EDV adjustment factor */
    uint16_t R0;                /* This value is the first order rate dependency factor, accounting for battery impedance adjustment. */
    uint16_t T0;                /* This value adjusts the variation of impedance with battery temperature. */
    uint16_t R1;                /* This value adjusts the variation of impedance with battery capacity. */
    uint8_t  TC;                /* This value adjusts the variation of impedance for cold temperatures (T < 23°C). */
    uint8_t  C1;                /* This value is the desired reserved battery capacity remaining at EDV0. */

    /* Voltage vs Depth-of-Discharge (DOD) table (mV) */
    uint16_t DOD0;              /* 0 % DOD (full charge) */
    uint16_t DOD10;             /* 10 % DOD */
    uint16_t DOD20;             /* 20 % DOD */
    uint16_t DOD30;             /* 30 % DOD */
    uint16_t DOD40;             /* 40 % DOD */
    uint16_t DOD50;             /* 50 % DOD */
    uint16_t DOD60;             /* 60 % DOD */
    uint16_t DOD70;             /* 70 % DOD */
    uint16_t DOD80;             /* 80 % DOD */
    uint16_t DOD90;             /* 90 % DOD */
    uint16_t DOD100;            /* 100 % DOD (empty) */
} parameter_cedv_t;

typedef struct {
    i2c_bus_handle_t i2c_bus;  // I2C bus handle
    const gauging_config_t *cfg;  // Pointer to the gauging_config_t structure
    const parameter_cedv_t *cedv;  // Pointer to the CEDV structure
} bq27220_config_t;

typedef void *bq27220_handle_t;

/**
 * @brief Create a new BQ27220 handle
 *
 * @param config[in] Configuration structure containing I2C bus handle and gauge parameters
 *
 * @return bq27220_handle_t Handle to the BQ27220 device, or NULL if creation failed
 */
bq27220_handle_t bq27220_create(const bq27220_config_t *config);

/**
 * @brief Delete a BQ27220 handle
 *
 * @param bq_handle[in] Handle to the BQ27220 device
 *
 * @return
 *     - ESP_OK if successful
 *     - ESP_ERR_INVALID_ARG if bq_handle is NULL
 */
esp_err_t bq27220_delete(bq27220_handle_t bq_handle);

/**
 * @brief Get the firmware version
 *
 * @param bq_handle[in] Handle to the BQ27220 device
 *
 * @return uint16_t Firmware version
 */
uint16_t bq27220_get_fw_version(bq27220_handle_t bq_handle);

/**
 * @brief Get the hardware version
 *
 * @param bq_handle[in] Handle to the BQ27220 device
 *
 * @return uint16_t Hardware version
 */
uint16_t bq27220_get_hw_version(bq27220_handle_t bq_handle);

/**
 * @brief Seal the BQ27220 device to prevent unauthorized access to configuration data
 *
 * @param bq_handle[in] Handle to the BQ27220 device
 *
 * @return
 *     - ESP_OK if successful
 *     - ESP_ERR_INVALID_ARG if bq_handle is NULL
 *     - ESP_FAIL if operation failed
 */
esp_err_t bq27220_seal(bq27220_handle_t bq_handle);

/**
 * @brief Unseal the BQ27220 device to allow access to configuration data
 *
 * @param bq_handle[in] Handle to the BQ27220 device
 *
 * @return
 *     - ESP_OK if successful
 *     - ESP_ERR_INVALID_ARG if bq_handle is NULL
 *     - ESP_FAIL if operation failed
 */
esp_err_t bq27220_unseal(bq27220_handle_t bq_handle);

/**
 * @brief Set a 16-bit parameter in the BQ27220 device
 *
 * @param bq_handle[in] Handle to the BQ27220 device
 * @param address[in] Parameter address
 * @param value[in] Value to set
 *
 * @return
 *     - ESP_OK if successful
 *     - ESP_ERR_INVALID_ARG if bq_handle is NULL
 *     - ESP_FAIL if operation failed
 */
esp_err_t bq27220_set_parameter_u16(bq27220_handle_t bq_handle, uint16_t address, uint16_t value);

/**
 * @brief Get a 16-bit parameter from the BQ27220 device
 *
 * @param bq_handle[in] Handle to the BQ27220 device
 * @param address[in] Parameter address
 *
 * @return uint16_t Parameter value
 */
uint16_t bq27220_get_parameter_u16(bq27220_handle_t bq_handle, uint16_t address);

/**
 * @brief Get the battery voltage in millivolts (mV)
 *
 * @param handle[in] Handle to the BQ27220 device
 *
 * @return uint16_t Battery voltage in mV
 */
uint16_t bq27220_get_voltage(bq27220_handle_t handle);

/**
 * @brief Get the instantaneous current in milliamperes (mA)
 *
 * @param handle[in] Handle to the BQ27220 device
 *
 * @return int16_t Battery current in mA (positive for charging, negative for discharging)
 */
int16_t bq27220_get_current(bq27220_handle_t handle);

/**
 * @brief Get the average current in milliamperes (mA)
 *
 * @param bq_handle[in] Handle to the BQ27220 device
 *
 * @return int16_t Average battery current in mA (positive for charging, negative for discharging)
 */
int16_t bq27220_get_avgcurrent(bq27220_handle_t bq_handle);

/**
 * @brief Get the battery cycle count
 *
 * @param bq_handle[in] Handle to the BQ27220 device
 *
 * @return uint16_t Number of charge/discharge cycles
 */
uint16_t bq27220_get_cycle_count(bq27220_handle_t bq_handle);

/**
 * @brief Get the battery status
 *
 * @param handle[in] Handle to the BQ27220 device
 * @param battery_status[out] Pointer to store the battery status
 *
 * @return
 *     - ESP_OK if successful
 *     - ESP_ERR_INVALID_ARG if handle or battery_status is NULL
 */
esp_err_t bq27220_get_battery_status(bq27220_handle_t handle, battery_status_t *battery_status);

/**
 * @brief Get the operation status
 *
 * @param handle[in] Handle to the BQ27220 device
 * @param operation_status[out] Pointer to store the operation status
 *
 * @return
 *     - ESP_OK if successful
 *     - ESP_ERR_INVALID_ARG if handle or operation_status is NULL
 */
esp_err_t bq27220_get_operation_status(bq27220_handle_t handle, operation_status_t *operation_status);

/**
 * @brief Get the temperature in units of 0.1°K
 *
 * @param handle[in] Handle to the BQ27220 device
 *
 * @return uint16_t Temperature in 0.1°K
 */
uint16_t bq27220_get_temperature(bq27220_handle_t handle);

/**
 * @brief Get the compensated full charge capacity in mAh
 *
 * @param handle[in] Handle to the BQ27220 device
 *
 * @return uint16_t Full charge capacity in mAh
 */
uint16_t bq27220_get_full_charge_capacity(bq27220_handle_t handle);

/**
 * @brief Get the design capacity in mAh
 *
 * @param handle[in] Handle to the BQ27220 device
 *
 * @return uint16_t Design capacity in mAh
 */
uint16_t bq27220_get_design_capacity(bq27220_handle_t handle);

/**
 * @brief Get the remaining capacity in mAh
 *
 * @param handle[in] Handle to the BQ27220 device
 *
 * @return uint16_t Remaining capacity in mAh
 */
uint16_t bq27220_get_remaining_capacity(bq27220_handle_t handle);

/**
 * @brief Get the predicted remaining battery capacity in percents
 *
 * @param handle[in] Handle to the BQ27220 device
 *
 * @return uint16_t State of charge in percent (0-100%)
 */
uint16_t bq27220_get_state_of_charge(bq27220_handle_t handle);

/**
 * @brief Get the ratio of full charge capacity over design capacity in percents
 *
 * @param handle[in] Handle to the BQ27220 device
 *
 * @return uint16_t State of health in percent (0-100%)
 */
uint16_t bq27220_get_state_of_health(bq27220_handle_t handle);

/**
 * @brief Get the charge voltage in millivolts (mV)
 *
 * @param bq_handle[in] Handle to the BQ27220 device
 *
 * @return uint16_t Charge voltage in mV
 */
uint16_t bq27220_get_charge_voltage(bq27220_handle_t bq_handle);

/**
 * @brief Get the charge current in milliamperes (mA)
 *
 * @param bq_handle[in] Handle to the BQ27220 device
 *
 * @return uint16_t Charge current in mA
 */
uint16_t bq27220_get_charge_current(bq27220_handle_t bq_handle);

/**
 * @brief Get the average power in milliwatts (mW)
 *
 * @param bq_handle[in] Handle to the BQ27220 device
 *
 * @return int16_t Average power in mW (positive for charging, negative for discharging)
 */
int16_t bq27220_get_average_power(bq27220_handle_t bq_handle);

/**
 * @brief Get the predicted time to empty in minutes
 *
 * @param bq_handle[in] Handle to the BQ27220 device
 *
 * @return uint16_t Time to empty in minutes
 */
uint16_t bq27220_get_time_to_empty(bq27220_handle_t bq_handle);

/**
 * @brief Get the predicted time to full in minutes
 *
 * @param bq_handle[in] Handle to the BQ27220 device
 *
 * @return uint16_t Time to full in minutes
 */
uint16_t bq27220_get_time_to_full(bq27220_handle_t bq_handle);

/**
 * @brief Get the maximum load current in milliamperes (mA)
 *
 * @param bq_handle[in] Handle to the BQ27220 device
 *
 * @return int16_t Maximum load current in mA
 */
int16_t bq27220_get_maxload_current(bq27220_handle_t bq_handle);

/**
 * @brief Get the standby current in milliamperes (mA)
 *
 * @param bq_handle[in] Handle to the BQ27220 device
 *
 * @return int16_t Standby current in mA
 */
int16_t bq27220_get_standby_current(bq27220_handle_t bq_handle);

#ifdef __cplusplus
}
#endif
