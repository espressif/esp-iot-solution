/*
 * SPDX-FileCopyrightText: 2023-2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <map>
#include "esp_hal_bldc_3pwm.h"
#include "esp_hal_bldc_6pwm.h"
#include "esp_idf_version.h"
#include "soc/soc_caps.h"

#ifdef CONFIG_SOC_MCPWM_SUPPORTED
#if (ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(6, 0, 0))
#include "hal/mcpwm_periph.h"
#endif
#define MCPWM_TIMER_CLK_SRC MCPWM_TIMER_CLK_SRC_DEFAULT
#if (ESP_IDF_VERSION < ESP_IDF_VERSION_VAL(6, 0, 0))
#define ESP_SIMPLEFOC_MCPWM_GROUPS SOC_MCPWM_GROUPS
#else
#define ESP_SIMPLEFOC_MCPWM_GROUPS (sizeof(soc_mcpwm_signals) / sizeof(soc_mcpwm_signals[0]))
#endif
#endif

static std::vector<std::pair<DriverMode, std::tuple<int, int>>> createHardwareResource()
{
    std::vector<std::pair<DriverMode, std::tuple<int, int>>> resources;
#ifdef CONFIG_SOC_MCPWM_SUPPORTED
    for (int group_id = 0; group_id < ESP_SIMPLEFOC_MCPWM_GROUPS; group_id++) {
        resources.push_back({DriverMode::mcpwm, {group_id, 0}});
    }
#endif

    for (int channel = 0; channel < SOC_LEDC_CHANNEL_NUM; channel++) {
        resources.push_back({DriverMode::ledc, {channel, 0}});
    }

#ifdef CONFIG_SOC_MCPWM_SUPPORTED
    printf("BLDC 3PWM hardware resource created: MCPWM groups=%d, LEDC channels=%d\n", ESP_SIMPLEFOC_MCPWM_GROUPS, SOC_LEDC_CHANNEL_NUM);
#else
    printf("BLDC 3PWM hardware resource created: MCPWM groups=0, LEDC channels=%d\n", SOC_LEDC_CHANNEL_NUM);
#endif

    return resources;
}

std::vector<std::pair<DriverMode, std::tuple<int, int>>> HardwareResource = createHardwareResource();

enum class DriverSelectionType {
    none = 0,
#ifdef CONFIG_SOC_MCPWM_SUPPORTED
    mcpwm,
#endif
    ledc,
};

struct DriverSelection {
    DriverSelectionType type = DriverSelectionType::none;
    int mcpwm_group = -1;
    std::vector<int> ledc_channels;
};

#ifdef CONFIG_SOC_MCPWM_SUPPORTED
extern bool IRAM_ATTR esp_simplefoc_lowside_on_mcpwm_full(mcpwm_timer_handle_t timer,
                                                          const mcpwm_timer_event_data_t *edata,
                                                          void *user_data);
#endif

BLDCDriver3PWM::BLDCDriver3PWM(int phA, int phB, int phC, int en1, int en2, int en3)
{
    // Pin initialization
    pwmA = phA;
    pwmB = phB;
    pwmC = phC;

    // enable_pin pin
    enableA_pin = en1;
    enableB_pin = en2;
    enableC_pin = en3;

    // default power-supply value
    voltage_power_supply = DEF_POWER_SUPPLY;
    voltage_limit = NOT_SET;
    pwm_frequency = NOT_SET;
}

void BLDCDriver3PWM::enable()
{
    // enable_pin the driver - if enable_pin pin available
    if (_isset(enableA_pin)) {
        digitalWrite(enableA_pin, enable_active_high);
    }
    if (_isset(enableB_pin)) {
        digitalWrite(enableB_pin, enable_active_high);
    }
    if (_isset(enableC_pin)) {
        digitalWrite(enableC_pin, enable_active_high);
    }
    // set zero to PWM
    setPwm(0, 0, 0);
}

void BLDCDriver3PWM::disable()
{
    // set zero to PWM
    setPwm(0, 0, 0);
    // disable the driver - if enable_pin pin available
    if (_isset(enableA_pin)) {
        digitalWrite(enableA_pin, !enable_active_high);
    }
    if (_isset(enableB_pin)) {
        digitalWrite(enableB_pin, !enable_active_high);
    }
    if (_isset(enableC_pin)) {
        digitalWrite(enableC_pin, !enable_active_high);
    }
}

void BLDCDriver3PWM::setPhaseState(PhaseState sa, PhaseState sb, PhaseState sc)
{
    // disable if needed
    if (_isset(enableA_pin) && _isset(enableB_pin) && _isset(enableC_pin)) {
        digitalWrite(enableA_pin, sa == PhaseState::PHASE_ON ? enable_active_high : !enable_active_high);
        digitalWrite(enableB_pin, sb == PhaseState::PHASE_ON ? enable_active_high : !enable_active_high);
        digitalWrite(enableC_pin, sc == PhaseState::PHASE_ON ? enable_active_high : !enable_active_high);
    }
}

// Set voltage to the pwm pin
void BLDCDriver3PWM::setPwm(float Ua, float Ub, float Uc)
{

    // limit the voltage in driver
    Ua = _constrain(Ua, 0.0f, voltage_limit);
    Ub = _constrain(Ub, 0.0f, voltage_limit);
    Uc = _constrain(Uc, 0.0f, voltage_limit);
    // calculate duty cycle
    // limited in [0,1]
    dc_a = _constrain(Ua / voltage_power_supply, 0.0f, 1.0f);
    dc_b = _constrain(Ub / voltage_power_supply, 0.0f, 1.0f);
    dc_c = _constrain(Uc / voltage_power_supply, 0.0f, 1.0f);

    // hardware specific writing
    // hardware specific function - depending on driver and mcu
    //_writeDutyCycle3PWM(dc_a, dc_b, dc_c, params);

    halPwmWrite();
}

bool checkMcpwmGroupAvailable(int group_id)
{
    for (const auto &kvp : HardwareResource) {
        if (kvp.first == DriverMode::mcpwm) {
            int mcpwm_group = std::get<0>(kvp.second);
            int mcpwm_used = std::get<1>(kvp.second);

            if (mcpwm_group == group_id) {
                return !mcpwm_used;
            }
        }
    }
    return false;
}

bool checkLedcAvailable(std::vector<int> ledc_channels)
{
    int channeld_available_num = 0;
    if (ledc_channels.size() != 3) {
        printf("The number of channels is incorrect and should be set to 3");
        return false;
    }
    if (ledc_channels[0] == ledc_channels[1] || ledc_channels[0] == ledc_channels[2] || ledc_channels[1] == ledc_channels[2]) {
        printf("The LEDC channels should be different\n");
        return false;
    }

    for (auto channel : ledc_channels) {
        for (const auto &kvp : HardwareResource) {
            if (kvp.first == DriverMode::ledc) {
                int ledc_channel = std::get<0>(kvp.second);
                int ledc_channel_used = std::get<1>(kvp.second);
                if (channel == ledc_channel) {
                    if (!ledc_channel_used) {
                        channeld_available_num++;
                    }
                }
            }
        }
    }

    return channeld_available_num == 3;
}

DriverSelection checkAvailableDriver()
{
    DriverSelection selection;
#ifdef CONFIG_SOC_MCPWM_SUPPORTED
    // find mcpwm
    for (auto &kvp : HardwareResource) {
        if (kvp.first == DriverMode::mcpwm) {
            if (std::get<1>(kvp.second) == 0) {
                selection.mcpwm_group = std::get<0>(kvp.second);
                break;
            }
        }
    }

    if (selection.mcpwm_group != -1) {
        printf("MCPWM Group: %d is idle\n", selection.mcpwm_group);
        selection.type = DriverSelectionType::mcpwm;
        return selection;
    }

    printf("no available mcpwm driver\n");
#endif

    // find ledc
    for (auto &kvp : HardwareResource) {
        if (kvp.first == DriverMode::ledc) {
            if (std::get<1>(kvp.second) == 0 && selection.ledc_channels.size() < 3) {
                selection.ledc_channels.push_back(std::get<0>(kvp.second));
                continue;
            }
        }
    }

    if (selection.ledc_channels.size() == 3) {
        printf("Ledc channel: %d %d %d is idle\n", selection.ledc_channels[0], selection.ledc_channels[1], selection.ledc_channels[2]);
        selection.type = DriverSelectionType::ledc;
        return selection;
    }

    printf("no available ledc driver\n");

    return selection;
}

void setMcpwmGroupUsed(int group_id)
{
    for (auto &kvp : HardwareResource) {
        if (kvp.first == DriverMode::mcpwm) {
            int mcpwm_group = std::get<0>(kvp.second);
            if (mcpwm_group == group_id) {
                kvp.second = {mcpwm_group, 1};
            }
        }
    }
}

void setMcpwmGroupUnUsed(int group_id)
{
    for (auto &kvp : HardwareResource) {
        if (kvp.first == DriverMode::mcpwm) {
            int mcpwm_group = std::get<0>(kvp.second);
            if (mcpwm_group == group_id) {
                kvp.second = {mcpwm_group, 0};
            }
        }
    }
}

void setLedcChannelUsed(std::vector<int> ledc_channels)
{
    for (auto channel : ledc_channels) {
        for (auto &kvp : HardwareResource) {
            if (kvp.first == DriverMode::ledc) {
                int ledc_channel = std::get<0>(kvp.second);
                if (channel == ledc_channel) {
                    kvp.second = {channel, 1};
                }
            }
        }
    }
}

void setLedcChannelUnUsed(std::vector<int> ledc_channels)
{
    for (auto channel : ledc_channels) {
        for (auto &kvp : HardwareResource) {
            if (kvp.first == DriverMode::ledc) {
                int ledc_channel = std::get<0>(kvp.second);
                if (channel == ledc_channel) {
                    kvp.second = {channel, 0};
                }
            }
        }
    }
}

void BLDCDriver3PWM::preparePinsAndVoltage()
{
    // PWM pins
    pinMode(pwmA, OUTPUT);
    pinMode(pwmB, OUTPUT);
    pinMode(pwmC, OUTPUT);
    if (_isset(enableA_pin)) {
        pinMode(enableA_pin, OUTPUT);
    }
    if (_isset(enableB_pin)) {
        pinMode(enableB_pin, OUTPUT);
    }
    if (_isset(enableC_pin)) {
        pinMode(enableC_pin, OUTPUT);
    }

    // sanity check for the voltage limit configuration
    if (!_isset(voltage_limit) || voltage_limit > voltage_power_supply) {
        voltage_limit = voltage_power_supply;
    }
}

int BLDCDriver3PWM::init()
{
    preparePinsAndVoltage();

    // Set the pwm frequency to the pins
    // hardware specific function - depending on driver and mcu
    // params = _configure3PWM(pwm_frequency, pwmA, pwmB, pwmC);
    // initialized = (params != SIMPLEFOC_DRIVER_INIT_FAILED);
    // return params != SIMPLEFOC_DRIVER_INIT_FAILED;

    // ESP32 Platform specific function. Auto select driver.

    DriverSelection selection = checkAvailableDriver();
    if (selection.type == DriverSelectionType::none) {
        initialized = 0;
        printf("No available Driver.\n");
        return 0;
    }
#ifdef CONFIG_SOC_MCPWM_SUPPORTED
    if (selection.type == DriverSelectionType::mcpwm) {
        // mcpwm
        driverMode = DriverMode::mcpwm;
        mcpwm_group = selection.mcpwm_group;
        setMcpwmGroupUsed(selection.mcpwm_group); // mark this group is used.
        printf("Auto. Current Driver uses Mcpwm GroupId:%d\n", selection.mcpwm_group);

        // Init mcpwm driver.
        mcpwm_timer_config_t timer_config = {
            .group_id = selection.mcpwm_group,
            .clk_src = MCPWM_TIMER_CLK_SRC,
            .resolution_hz = _PWM_TIMEBASE_RESOLUTION_HZ,
            .count_mode = MCPWM_TIMER_COUNT_MODE_UP,                                      // centeral mode
            .period_ticks = (uint32_t)(1 * _PWM_TIMEBASE_RESOLUTION_HZ / _PWM_FREQUENCY), // real frequency is pwm_frequency/2
        };
        ESP_ERROR_CHECK(mcpwm_new_timer(&timer_config, &timer));

        mcpwm_operator_config_t operator_config = {
            .group_id = selection.mcpwm_group, // operator must be in the same group to the timer
        };

        for (int i = 0; i < 3; i++) {
            ESP_ERROR_CHECK(mcpwm_new_operator(&operator_config, &oper[i]));
        }

        for (int i = 0; i < 3; i++) {
            ESP_ERROR_CHECK(mcpwm_operator_connect_timer(oper[i], timer));
        }

        mcpwm_comparator_config_t comparator_config = {0};
        comparator_config.intr_priority = 0;
        comparator_config.flags.update_cmp_on_tez = true;
        comparator_config.flags.update_cmp_on_tep = false;
        comparator_config.flags.update_cmp_on_sync = false;

        for (int i = 0; i < 3; i++) {
            ESP_ERROR_CHECK(mcpwm_new_comparator(oper[i], &comparator_config, &comparator[i]));
            ESP_ERROR_CHECK(mcpwm_comparator_set_compare_value(comparator[i], (0)));
        }

        mcpwm_generator_config_t generator_config = {};
        int gen_gpios[3] = {pwmA, pwmB, pwmC}; // three gpio port

        for (int i = 0; i < 3; i++) {
            generator_config.gen_gpio_num = gen_gpios[i];
            ESP_ERROR_CHECK(mcpwm_new_generator(oper[i], &generator_config, &generator[i]));
        }

        for (int i = 0; i < 3; i++) {
            ESP_ERROR_CHECK(mcpwm_generator_set_action_on_timer_event(generator[i],
                                                                      MCPWM_GEN_TIMER_EVENT_ACTION(MCPWM_TIMER_DIRECTION_UP, MCPWM_TIMER_EVENT_EMPTY, MCPWM_GEN_ACTION_HIGH)));
            // go low on compare threshold
            ESP_ERROR_CHECK(mcpwm_generator_set_action_on_compare_event(generator[i],
                                                                        MCPWM_GEN_COMPARE_EVENT_ACTION(MCPWM_TIMER_DIRECTION_UP, comparator[i], MCPWM_GEN_ACTION_LOW)));
        }
        EspMcpwmDriverParams *driver_params = new EspMcpwmDriverParams;
        driver_params->group_id = selection.mcpwm_group;
        driver_params->timer = timer;
        driver_params->period_ticks = timer_config.period_ticks;
        driver_params->center_aligned = false;
        for (int i = 0; i < 3; i++) {
            driver_params->oper[i] = oper[i];
            driver_params->comparator[i] = comparator[i];
            driver_params->generator[i][0] = generator[i];
        }
        mcpwm_timer_event_callbacks_t timer_cbs = {
            .on_full = esp_simplefoc_lowside_on_mcpwm_full,
        };
        ESP_ERROR_CHECK(mcpwm_timer_register_event_callbacks(timer, &timer_cbs, driver_params));
        params = driver_params;

        // enable mcpwm
        ESP_ERROR_CHECK(mcpwm_timer_enable(timer));
        ESP_ERROR_CHECK(mcpwm_timer_start_stop(timer, MCPWM_TIMER_START_NO_STOP));

        mcpwm_period = timer_config.period_ticks;
        initialized = 1;
    } else
#endif
        if (selection.type == DriverSelectionType::ledc) {
            // ledc
            driverMode = DriverMode::ledc;
            printf("Current Driver uses LEDC Channel:%d %d %d\n", selection.ledc_channels[0], selection.ledc_channels[1], selection.ledc_channels[2]);
            ledc_channels = selection.ledc_channels;
            setLedcChannelUsed(ledc_channels); //  mark this ledc channel is used.

            ledc_timer_config_t ledc_timer = {
                .speed_mode = _LEDC_MODE,
                .duty_resolution = _LEDC_DUTY_RES,
                .timer_num = LEDC_TIMER_0,
                .freq_hz = _LEDC_FREQUENCY, // Set output frequency at 20 kHz
#if CONFIG_IDF_TARGET_ESP32C6 || CONFIG_IDF_TARGET_ESP32H2
                .clk_cfg = LEDC_AUTO_CLK
#else
                .clk_cfg = LEDC_USE_APB_CLK
#endif
            };
            ESP_ERROR_CHECK(ledc_timer_config(&ledc_timer));

            int pins[3] = {pwmA, pwmB, pwmC}; // save pins

            ledc_channel_config_t ledc_channel = {};
            for (int i = 0; i < 3; i++) {
                ledc_channel.speed_mode = _LEDC_MODE;
                ledc_channel.timer_sel = LEDC_TIMER_0;
                ledc_channel.duty = 0;
                ledc_channel.hpoint = 0;
#if (ESP_IDF_VERSION < ESP_IDF_VERSION_VAL(6, 0, 0))
                ledc_channel.intr_type = LEDC_INTR_DISABLE;
#endif
                ledc_channel.channel = static_cast<ledc_channel_t>(ledc_channels[i]);
                ledc_channel.gpio_num = pins[i];
                ESP_ERROR_CHECK(ledc_channel_config(&ledc_channel));
            }

            initialized = 1;
            mcpwm_period = _LEDC_DUTY;
        }

    return 1;
}

#ifdef CONFIG_SOC_MCPWM_SUPPORTED
int BLDCDriver3PWM::init(int _mcpwm_group)
{
    preparePinsAndVoltage();

    // ESP32 Platform specific function. Using mcpwm driver
    driverMode = DriverMode::mcpwm;
    mcpwm_group = _mcpwm_group;
    if (checkMcpwmGroupAvailable(_mcpwm_group) == false) {
        printf("MCPWM GroupId:%d is not available\n", _mcpwm_group);
        initialized = 0;
        return 0;
    }
    setMcpwmGroupUsed(_mcpwm_group); // mark this group is used.
    printf("Current Driver uses Mcpwm GroupId:%d\n", _mcpwm_group);

    // Init mcpwm driver.
    mcpwm_timer_config_t timer_config = {
        .group_id = _mcpwm_group,
        .clk_src = MCPWM_TIMER_CLK_SRC,
        .resolution_hz = _PWM_TIMEBASE_RESOLUTION_HZ,
        .count_mode = MCPWM_TIMER_COUNT_MODE_UP,                                      // centeral mode
        .period_ticks = (uint32_t)(1 * _PWM_TIMEBASE_RESOLUTION_HZ / _PWM_FREQUENCY), // real frequency is pwm_frequency/2
    };
    ESP_ERROR_CHECK(mcpwm_new_timer(&timer_config, &timer));

    mcpwm_operator_config_t operator_config = {
        .group_id = _mcpwm_group, // operator must be in the same group to the timer
    };

    for (int i = 0; i < 3; i++) {
        ESP_ERROR_CHECK(mcpwm_new_operator(&operator_config, &oper[i]));
    }

    for (int i = 0; i < 3; i++) {
        ESP_ERROR_CHECK(mcpwm_operator_connect_timer(oper[i], timer));
    }

    mcpwm_comparator_config_t comparator_config = {0};
    comparator_config.intr_priority = 0;
    comparator_config.flags.update_cmp_on_tez = true;
    comparator_config.flags.update_cmp_on_tep = false;
    comparator_config.flags.update_cmp_on_sync = false;

    for (int i = 0; i < 3; i++) {
        ESP_ERROR_CHECK(mcpwm_new_comparator(oper[i], &comparator_config, &comparator[i]));
        ESP_ERROR_CHECK(mcpwm_comparator_set_compare_value(comparator[i], (0)));
    }

    mcpwm_generator_config_t generator_config = {};
    int gen_gpios[3] = {pwmA, pwmB, pwmC}; // three gpio port

    for (int i = 0; i < 3; i++) {
        generator_config.gen_gpio_num = gen_gpios[i];
        ESP_ERROR_CHECK(mcpwm_new_generator(oper[i], &generator_config, &generator[i]));
    }

    for (int i = 0; i < 3; i++) {
        ESP_ERROR_CHECK(mcpwm_generator_set_action_on_timer_event(generator[i],
                                                                  MCPWM_GEN_TIMER_EVENT_ACTION(MCPWM_TIMER_DIRECTION_UP, MCPWM_TIMER_EVENT_EMPTY, MCPWM_GEN_ACTION_HIGH)));
        // go low on compare threshold
        ESP_ERROR_CHECK(mcpwm_generator_set_action_on_compare_event(generator[i],
                                                                    MCPWM_GEN_COMPARE_EVENT_ACTION(MCPWM_TIMER_DIRECTION_UP, comparator[i], MCPWM_GEN_ACTION_LOW)));
    }
    EspMcpwmDriverParams *driver_params = new EspMcpwmDriverParams;
    driver_params->group_id = _mcpwm_group;
    driver_params->timer = timer;
    driver_params->period_ticks = timer_config.period_ticks;
    driver_params->center_aligned = false;
    for (int i = 0; i < 3; i++) {
        driver_params->oper[i] = oper[i];
        driver_params->comparator[i] = comparator[i];
        driver_params->generator[i][0] = generator[i];
    }
    mcpwm_timer_event_callbacks_t timer_cbs = {
        .on_full = esp_simplefoc_lowside_on_mcpwm_full,
    };
    ESP_ERROR_CHECK(mcpwm_timer_register_event_callbacks(timer, &timer_cbs, driver_params));
    params = driver_params;

    // enable mcpwm
    ESP_ERROR_CHECK(mcpwm_timer_enable(timer));
    ESP_ERROR_CHECK(mcpwm_timer_start_stop(timer, MCPWM_TIMER_START_NO_STOP));

    mcpwm_period = timer_config.period_ticks;
    initialized = 1;

    return 1;
}
#endif

int BLDCDriver3PWM::init(std::vector<int> _ledc_channels)
{
    preparePinsAndVoltage();

    // ESP32 Platform specific function. Using ledc driver
    driverMode = DriverMode::ledc;
    if (checkLedcAvailable(_ledc_channels) == false) {
        printf("LEDC Channels is not available\n");
        initialized = 0;
        return 0;
    }

    printf("Current Driver uses LEDC Channel:%d %d %d\n", _ledc_channels[0], _ledc_channels[1], _ledc_channels[2]);
    ledc_channels = _ledc_channels;
    setLedcChannelUsed(ledc_channels); //  mark this ledc channel is used.

    ledc_timer_config_t ledc_timer = {
        .speed_mode = _LEDC_MODE,
        .duty_resolution = _LEDC_DUTY_RES,
        .timer_num = LEDC_TIMER_0,
        .freq_hz = _LEDC_FREQUENCY, // Set output frequency at 20 kHz
#if CONFIG_IDF_TARGET_ESP32C6 || CONFIG_IDF_TARGET_ESP32H2
        .clk_cfg = LEDC_AUTO_CLK
#else
        .clk_cfg = LEDC_USE_APB_CLK
#endif
    };
    ESP_ERROR_CHECK(ledc_timer_config(&ledc_timer));

    int pins[3] = {pwmA, pwmB, pwmC}; // save pins

    ledc_channel_config_t ledc_channel = {};
    for (int i = 0; i < 3; i++) {
        ledc_channel.speed_mode = _LEDC_MODE;
        ledc_channel.timer_sel = LEDC_TIMER_0;
        ledc_channel.duty = 0;
        ledc_channel.hpoint = 0;
#if (ESP_IDF_VERSION < ESP_IDF_VERSION_VAL(6, 0, 0))
        ledc_channel.intr_type = LEDC_INTR_DISABLE;
#endif
        ledc_channel.channel = static_cast<ledc_channel_t>(ledc_channels[i]);
        ledc_channel.gpio_num = pins[i];
        ESP_ERROR_CHECK(ledc_channel_config(&ledc_channel));
    }

    initialized = 1;
    mcpwm_period = _LEDC_DUTY;

    return 1;
}

int BLDCDriver3PWM::deinit()
{
    switch (driverMode) {
    case DriverMode::ledc: {
        int pins[3] = {pwmA, pwmB, pwmC};
        for (int i = 0; i < 3; i++) {
            if (ledc_stop(_LEDC_MODE, static_cast<ledc_channel_t>(ledc_channels[i]), 0) != ESP_OK) {
                return 0;
            }
            if (gpio_reset_pin(static_cast<gpio_num_t>(pins[i])) != ESP_OK) {
                return 0;
            }
        }
        if (ledc_timer_pause(_LEDC_MODE, _LEDC_TIMER) != ESP_OK) {
            return 0;
        }
        ledc_timer_config_t ledc_timer = {
            .speed_mode = _LEDC_MODE,
            .timer_num = _LEDC_TIMER,
            .deconfigure = true,
        };
        if (ledc_timer_config(&ledc_timer) != ESP_OK) {
            return 0;
        }
        setLedcChannelUnUsed(ledc_channels);
        ledc_channels.clear();
        params = nullptr;
        break;
    }
#ifdef CONFIG_SOC_MCPWM_SUPPORTED
    case DriverMode::mcpwm:
        for (int i = 0; i < 3; i++) {
            if (mcpwm_del_generator(generator[i]) != ESP_OK) {
                return 0;
            }

            if (mcpwm_del_comparator(comparator[i]) != ESP_OK) {
                return 0;
            }

            if (mcpwm_del_operator(oper[i]) != ESP_OK) {
                return 0;
            }
        }
        if (mcpwm_timer_disable(timer) != ESP_OK) {
            return 0;
        }

        if (mcpwm_del_timer(timer) != ESP_OK) {
            return 0;
        }

        delete static_cast<EspMcpwmDriverParams *>(params);
        params = nullptr;
        setMcpwmGroupUnUsed(mcpwm_group);
        break;
#endif
    default:
        break;
    }

    initialized = 0;
    return 1;
}

void BLDCDriver3PWM::halPwmWrite()
{
    if (!initialized) {
        return;
    }

    switch (driverMode) {
    case DriverMode::ledc:
        ESP_ERROR_CHECK(ledc_set_duty(_LEDC_MODE, static_cast<ledc_channel_t>(ledc_channels[0]), (uint32_t)((mcpwm_period * dc_a))));
        ESP_ERROR_CHECK(ledc_update_duty(_LEDC_MODE, static_cast<ledc_channel_t>(ledc_channels[0])));
        ESP_ERROR_CHECK(ledc_set_duty(_LEDC_MODE, static_cast<ledc_channel_t>(ledc_channels[1]), (uint32_t)((mcpwm_period * dc_b))));
        ESP_ERROR_CHECK(ledc_update_duty(_LEDC_MODE, static_cast<ledc_channel_t>(ledc_channels[1])));
        ESP_ERROR_CHECK(ledc_set_duty(_LEDC_MODE, static_cast<ledc_channel_t>(ledc_channels[2]), (uint32_t)((mcpwm_period * dc_c))));
        ESP_ERROR_CHECK(ledc_update_duty(_LEDC_MODE, static_cast<ledc_channel_t>(ledc_channels[2])));
        break;
#ifdef CONFIG_SOC_MCPWM_SUPPORTED
    case DriverMode::mcpwm:
        ESP_ERROR_CHECK(mcpwm_comparator_set_compare_value(comparator[0], (uint32_t)((mcpwm_period * dc_a))));
        ESP_ERROR_CHECK(mcpwm_comparator_set_compare_value(comparator[1], (uint32_t)((mcpwm_period * dc_b))));
        ESP_ERROR_CHECK(mcpwm_comparator_set_compare_value(comparator[2], (uint32_t)((mcpwm_period * dc_c))));
        break;
#endif
    default:
        break;
    }
}
