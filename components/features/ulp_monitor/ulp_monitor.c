// Copyright 2015-2016 Espressif Systems (Shanghai) PTE LTD
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at

//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
#include <string.h>
#include "esp_log.h"
#include "esp32/ulp.h"
#include "esp_sleep.h"
#include "driver/adc.h"
#include "driver/rtc_io.h"
#include "soc/rtc_cntl_reg.h"
#include "soc/sens_reg.h"
#include "soc/rtc.h"
#include "iot_ulp_monitor.h"

#if CONFIG_ULP_COPROC_RESERVE_MEM
#define ULP_PROGRAM_SIZE    (CONFIG_ULP_COPROC_RESERVE_MEM/8)
#define ULP_PROGRAM_ADDR_LIMI   (CONFIG_ULP_COPROC_RESERVE_MEM / 8 - 6)
#define ULP_DATA_ADDR_LIMI      (CONFIG_ULP_COPROC_RESERVE_MEM / 4 - 6)

#define IOT_CHECK(tag, a, ret)  if(!(a)) {                                 \
        ESP_LOGE(tag,"%s:%d (%s)", __FILE__, __LINE__, __FUNCTION__);      \
        return (ret);                                                      \
        }
#define ERR_ASSERT(tag, param)  IOT_CHECK(tag, (param) == ESP_OK, ESP_FAIL)

static const char* TAG = "ulp_monitor";
static ulp_insn_t g_program[ULP_PROGRAM_SIZE];
static uint16_t g_program_len, g_program_addr, g_data_addr;

static esp_err_t ulp_add_subprogram(const ulp_insn_t sub_program[], size_t sub_program_size)
{
    if ((g_program_len + sub_program_size/sizeof(ulp_insn_t)) > ULP_PROGRAM_SIZE) {
        ESP_LOGE(TAG, "program is too long!");
        return ESP_FAIL;
    }
    ESP_LOGI(TAG, "length of added sub program:%d", sub_program_size / sizeof(ulp_insn_t));
    memcpy(g_program + g_program_len, sub_program, sub_program_size);
    g_program_len += (sub_program_size / sizeof(ulp_insn_t));
    return ESP_OK;
}

static esp_err_t ulp_add_monitor_program(ulp_insn_t read_reg_inst, int16_t low_threshold, int16_t high_threshold, uint8_t data_offset, uint8_t data_num, bool num_max_wake)
{
    const ulp_insn_t sub_program[] = {
        I_MOVI(R2, g_data_addr + data_offset),
        I_LD(R1, R2, data_num+1),
        I_ADDR(R1, R2, R1),
        read_reg_inst,
        I_ST(R3, R1, 0),
        I_MOVI(R1, high_threshold),
        I_SUBR(R1, R1, R3),
        M_BXF(1),
        I_MOVI(R1, low_threshold),
        I_SUBR(R1, R3, R1),
        M_BXF(1)
    };
    ERR_ASSERT(TAG, ulp_add_subprogram(sub_program, sizeof(sub_program)));
    if (num_max_wake) {
        const ulp_insn_t sub_program2[] = {
            I_LD(R1, R2, data_num+1),
            I_LD(R3, R2, data_num),
            I_ADDI(R1, R1, 1),
            I_SUBR(R3, R3, R1),
            M_BXZ(1),
            I_ST(R1, R2, data_num+1)
        };
        ERR_ASSERT(TAG, ulp_add_subprogram(sub_program2, sizeof(sub_program2)));
    }
    else {
        const ulp_insn_t sub_program2[] = {
            I_LD(R1, R2, data_num+1),
            I_LD(R3, R2, data_num),
            I_ADDI(R1, R1, 1),
            I_SUBR(R3, R3, R1),
            M_BXZ(2),
            I_ST(R1, R2, data_num+1),
            I_HALT(),
            M_LABEL(2),
            I_MOVI(R1, 0),
            I_ST(R1, R2, data_num+1),
        };
        ERR_ASSERT(TAG, ulp_add_subprogram(sub_program2, sizeof(sub_program2)));
    }
    return ESP_OK;
}

esp_err_t iot_ulp_monitor_init(uint16_t program_addr, uint16_t data_addr)
{
    if (program_addr > ULP_PROGRAM_ADDR_LIMI) {
        ESP_LOGE(TAG, "program address is to large!");
        return ESP_FAIL;
    }
    if (data_addr > ULP_DATA_ADDR_LIMI || data_addr <= program_addr) {
        ESP_LOGE(TAG, "data address is to large or smaller than program address!");
        return ESP_FAIL;
    }
    g_program_len = 0;
    g_data_addr = data_addr;
    g_program_addr = program_addr;
    const ulp_insn_t sub_program[] = {
        I_MOVI(R2, data_addr)
    };
    if ((g_program_len + sizeof(sub_program)/sizeof(ulp_insn_t)) >= ULP_PROGRAM_SIZE) {
        ESP_LOGE(TAG, "program is too long!");
        return ESP_FAIL;
    }
    memcpy(g_program + g_program_len, sub_program, sizeof(sub_program));
    g_program_len += (sizeof(sub_program) / sizeof(ulp_insn_t));
    return ESP_OK;
}

esp_err_t iot_ulp_add_adc_monitor(adc1_channel_t adc_chn, int16_t low_threshold, int16_t high_threshold, uint8_t data_offset, uint8_t data_num, bool num_max_wake)
{
    if ((g_data_addr + data_offset + data_num + 2) > ULP_DATA_ADDR_LIMI) {
        ESP_LOGE(TAG, "data_offset or data_num is to large");
        return ESP_FAIL;
    }

    CLEAR_PERI_REG_MASK(SENS_SAR_MEAS_START1_REG, SENS_SAR1_EN_PAD_FORCE);
    CLEAR_PERI_REG_MASK(SENS_SAR_MEAS_START2_REG, SENS_SAR2_EN_PAD_FORCE);
    SET_PERI_REG_BITS(SENS_SAR_MEAS_WAIT2_REG, SENS_FORCE_XPD_AMP_V, 0x2, SENS_FORCE_XPD_AMP_S);
    CLEAR_PERI_REG_MASK(SENS_SAR_MEAS_START1_REG, SENS_MEAS1_START_FORCE);
    SET_PERI_REG_BITS(SENS_SAR_MEAS_WAIT2_REG, SENS_FORCE_XPD_SAR_V, 3, SENS_FORCE_XPD_SAR_S);
    CLEAR_PERI_REG_MASK(SENS_SAR_MEAS_START1_REG, SENS_SAR1_EN_PAD_FORCE);
    CLEAR_PERI_REG_MASK(SENS_SAR_START_FORCE_REG, SENS_ULP_CP_START_TOP);
    CLEAR_PERI_REG_MASK(SENS_SAR_START_FORCE_REG, SENS_ULP_CP_FORCE_START_TOP);
    SET_PERI_REG_MASK(SENS_SAR_READ_CTRL_REG, SENS_SAR1_DATA_INV);
    SET_PERI_REG_MASK(SENS_SAR_READ_CTRL2_REG, SENS_SAR2_DATA_INV);
    iot_ulp_data_write(g_data_addr + data_offset + data_num, data_num);
    iot_ulp_data_write(g_data_addr + data_offset + data_num + 1, 0);
    ERR_ASSERT(TAG, ulp_add_monitor_program((ulp_insn_t)I_ADC(R3, 0, adc_chn), low_threshold, high_threshold, data_offset, data_num, num_max_wake)); 
    return ESP_OK;
}

esp_err_t iot_ulp_add_temprature_monitor(int16_t low_threshold, int16_t high_threshold, uint8_t data_offset, uint8_t data_num, bool num_max_wake)
{
    if ((g_data_addr + data_offset + data_num + 2) > ULP_DATA_ADDR_LIMI) {
        ESP_LOGE(TAG, "data_offset or data_num is to large");
        return ESP_FAIL;
    }
    SET_PERI_REG_BITS(SENS_SAR_TSENS_CTRL_REG, SENS_TSENS_CLK_DIV_V, 2, SENS_TSENS_CLK_DIV_S);
    SET_PERI_REG_BITS(SENS_SAR_MEAS_WAIT2_REG, SENS_FORCE_XPD_SAR_V, 3, SENS_FORCE_XPD_SAR_S);
    CLEAR_PERI_REG_MASK(SENS_SAR_TSENS_CTRL_REG, SENS_TSENS_POWER_UP);
    CLEAR_PERI_REG_MASK(SENS_SAR_TSENS_CTRL_REG, SENS_TSENS_DUMP_OUT);
    CLEAR_PERI_REG_MASK(SENS_SAR_TSENS_CTRL_REG, SENS_TSENS_POWER_UP_FORCE);
    iot_ulp_data_write(g_data_addr + data_offset + data_num, data_num);
    iot_ulp_data_write(g_data_addr + data_offset + data_num + 1, 0);
    ERR_ASSERT(TAG, ulp_add_monitor_program((ulp_insn_t)I_TSENS(R3, 8000), low_threshold, high_threshold, data_offset, data_num, num_max_wake));
    return ESP_OK;    
}

esp_err_t iot_ulp_monitor_start(uint32_t meas_per_hour)
{
    IOT_CHECK(TAG, meas_per_hour != 0, ESP_FAIL);
    const ulp_insn_t sub_program[] = {
        I_HALT(),
        M_LABEL(1),
        I_WAKE(),
        I_HALT()
    };
    ERR_ASSERT(TAG, ulp_add_subprogram(sub_program, sizeof(sub_program)));
    size_t size = g_program_len;
    esp_sleep_enable_ulp_wakeup();
    ERR_ASSERT(TAG, ulp_process_macros_and_load(g_program_addr, g_program, &size));
    const uint32_t sleep_cycles = rtc_clk_slow_freq_get_hz() * 3600 / meas_per_hour;
    REG_WRITE(SENS_ULP_CP_SLEEP_CYC0_REG, sleep_cycles);
    ERR_ASSERT(TAG, ulp_run(g_program_addr));
    return ESP_OK;
}

uint16_t iot_ulp_data_read(size_t addr)
{
    if (addr >= ULP_DATA_ADDR_LIMI) {
        ESP_LOGE("ulp_monitor", "ulp data read address is too large");
        return 0xffff;
    }
    return RTC_SLOW_MEM[addr] & 0xffff;
}

void iot_ulp_data_write(size_t addr, uint16_t value)
{
    if (addr >= ULP_DATA_ADDR_LIMI) {
        ESP_LOGE("ulp_monitor", "ulp data write address is too large");
        return;
    }
    RTC_SLOW_MEM[addr] = value;
    return;
}
#endif
