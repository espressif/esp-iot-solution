/*
 * SPDX-FileCopyrightText: 2023-2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include "BLDCDriver.h"
#include "foc_utils.h"
#include "time_utils.h"
#include "defaults.h"
#include "drivers/hardware_api.h"
#include "driver/mcpwm_prelude.h"
#include "driver/ledc.h"
#include <vector>

#define _PWM_FREQUENCY 20000                           // default
#define _PWM_FREQUENCY_MAX 50000                       // max
#define _PWM_TIMEBASE_RESOLUTION_HZ (10 * 1000 * 1000) // 1MHZ 1us per tick
#define _LEDC_TIMER LEDC_TIMER_0
#define _LEDC_MODE LEDC_LOW_SPEED_MODE
#define _LEDC_DUTY_RES LEDC_TIMER_9_BIT // Set duty resolution to 9 bits
#define _LEDC_DUTY (511)                // Duty Max = 2^9 = 511
#define _LEDC_FREQUENCY (20 * 1000)     // Frequency in Hertz. Set frequency at 20 kHz

enum class DriverMode
{
  mcpwm = 0,
  ledc = 1,
};

class BLDCDriver3PWM : public BLDCDriver
{
public:
  /**
    BLDCDriver class constructor
    @param phA A phase pwm pin
    @param phB B phase pwm pin
    @param phC C phase pwm pin
    @param en1 enable pin (optional input)
    @param en2 enable pin (optional input)
    @param en3 enable pin (optional input)
  */
  BLDCDriver3PWM(int phA, int phB, int phC, int en1 = NOT_SET, int en2 = NOT_SET, int en3 = NOT_SET);

  /**  Motor hardware init function */
  int init() override;
  /** Motor disable function */
  void disable() override;
  /** Motor enable function */
  void enable() override;

  // hardware variables
  int pwmA;        //!< phase A pwm pin number
  int pwmB;        //!< phase B pwm pin number
  int pwmC;        //!< phase C pwm pin number
  int enableA_pin; //!< enable pin number
  int enableB_pin; //!< enable pin number
  int enableC_pin; //!< enable pin number
  bool enable_active_high = true;

  /**
   * Set phase voltages to the harware
   *
   * @param Ua - phase A voltage
   * @param Ub - phase B voltage
   * @param Uc - phase C voltage
   */
  void setPwm(float Ua, float Ub, float Uc) override;

  /**
   * Set phase voltages to the harware
   *
   * @param sc - phase A state : active / disabled ( high impedance )
   * @param sb - phase B state : active / disabled ( high impedance )
   * @param sa - phase C state : active / disabled ( high impedance )
   */
  virtual void setPhaseState(PhaseState sa, PhaseState sb, PhaseState sc) override;

  /*ESP32 Platform Function*/
  int init(int _mcpwm_group);
  int init(std::vector<int> _ledc_channels);

  void halPwmWrite();

private:
  DriverMode driverMode;
  int mcpwm_group;
  std::vector<int> ledc_channels;
  mcpwm_cmpr_handle_t comparator[3];
  uint32_t mcpwm_period;
};
