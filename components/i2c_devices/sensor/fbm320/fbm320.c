// Ported over by Serena Yeoh (Nov 2019)
// for the ESP32 Azure IoT Kit by Espressif Systems (Shanghai) PTE LTD
//
// References:
//  Datasheet
//  http://www.fmti.com.tw/en/product_c_p_1
//
//  Original driver from Formosa (manufacturer)
//  https://github.com/formosa-measurement-technology-inc/FMTI_fbm320_driver
//
//  Sample implementation of drivers for the ESP IoT Solutions
//  https://github.com/espressif/esp-iot-solution/tree/master/components/i2c_devices/sensor
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
#include <stdio.h>
#include <time.h>
#include <sys/time.h>
#include "esp_system.h"
#include "driver/i2c.h"
#include "iot_i2c_bus.h"
#include "iot_fbm320.h"
#include "esp_log.h"

typedef struct {
    i2c_bus_handle_t bus;
    uint16_t dev_addr;
    fbm320_calibration_data_t calibration_data;
    fbm320_oversampling_rate oversampling_rate;
    uint8_t cmd_pressure;
    uint32_t time_pressure;
    uint32_t raw_temperature;
	uint32_t raw_pressure;
    float temperature;
	int32_t pressure;
} fbm320_dev_t;

fbm320_handle_t iot_fbm320_create(i2c_bus_handle_t bus, uint16_t dev_addr)
{
    fbm320_dev_t* sensor = (fbm320_dev_t*) calloc(1, sizeof(fbm320_dev_t));
    sensor->bus = bus;
    sensor->dev_addr = dev_addr;

    return (fbm320_handle_t) sensor;
}

esp_err_t iot_fbm320_delete(fbm320_handle_t sensor, bool del_bus)
{
    fbm320_dev_t* sens = (fbm320_dev_t*) sensor;
    if(del_bus) 
    {
        iot_i2c_bus_delete(sens->bus);
        sens->bus = NULL;
    }
    free(sens);
    return ESP_OK;
}

esp_err_t iot_fbm320_read_byte(fbm320_handle_t sensor, uint8_t reg_addr, uint8_t *data)
{
    esp_err_t ret;
    fbm320_dev_t* sens = (fbm320_dev_t*) sensor;

    i2c_cmd_handle_t cmd = i2c_cmd_link_create();
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, (sens->dev_addr << 1) | WRITE_BIT, ACK_CHECK_EN);
    i2c_master_write_byte(cmd, reg_addr, ACK_CHECK_EN);

    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, (sens->dev_addr << 1) | READ_BIT, ACK_CHECK_EN);
    i2c_master_read_byte(cmd, data, NACK_VAL);
    i2c_master_stop(cmd);
    ret = iot_i2c_bus_cmd_begin(sens->bus, cmd, 1000 / portTICK_RATE_MS);
    i2c_cmd_link_delete(cmd);

    return ret;
}

esp_err_t iot_fbm320_read(fbm320_handle_t sensor, uint8_t reg_addr, uint8_t reg_num, uint8_t* data_buf)
{
    esp_err_t ret;
    fbm320_dev_t* sens = (fbm320_dev_t*) sensor;

    uint8_t max = reg_num - 1; 
    if (data_buf != NULL && reg_num > 0) 
    {
        i2c_cmd_handle_t cmd = i2c_cmd_link_create();

        i2c_master_start(cmd);
        i2c_master_write_byte(cmd, (sens->dev_addr << 1) | WRITE_BIT, ACK_CHECK_EN);
        i2c_master_write_byte(cmd, reg_addr, ACK_CHECK_EN);

        i2c_master_start(cmd);
        i2c_master_write_byte(cmd, (sens->dev_addr << 1) | READ_BIT, ACK_CHECK_EN);
        i2c_master_read(cmd, data_buf, max, ACK_VAL);
        i2c_master_read(cmd, data_buf + max, 1, NACK_VAL);
        i2c_master_stop(cmd);
        ret = iot_i2c_bus_cmd_begin(sens->bus, cmd, 1000 / portTICK_RATE_MS);
        i2c_cmd_link_delete(cmd);
        
        if (ret == ESP_FAIL) return ret;
    }

    return ESP_OK;
}

esp_err_t iot_fbm320_write_byte(fbm320_handle_t sensor, uint8_t reg_addr, uint8_t data)
{
    fbm320_dev_t* sens = (fbm320_dev_t*) sensor;
    esp_err_t  ret;
    i2c_cmd_handle_t cmd = i2c_cmd_link_create();
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, (sens->dev_addr << 1) | WRITE_BIT, ACK_CHECK_EN);
    i2c_master_write_byte(cmd, reg_addr, ACK_CHECK_EN);
    i2c_master_write_byte(cmd, data, ACK_CHECK_EN);
    ret = iot_i2c_bus_cmd_begin(sens->bus, cmd, 1000 / portTICK_RATE_MS);
    i2c_cmd_link_delete(cmd);

    if (ret == ESP_FAIL) return ret;

    return ESP_OK;
}

esp_err_t iot_fbm320_write(fbm320_handle_t sensor, uint8_t reg_start_addr, uint8_t reg_num, uint8_t *data_buf)
{
    uint32_t i = 0;
    if (data_buf != NULL) {
        for(i=0; i<reg_num; i++) {
            iot_fbm320_write_byte(sensor, reg_start_addr+i, data_buf[i]);
        }
        return ESP_OK;
    }
    return ESP_FAIL;
}

esp_err_t iot_fbm320_get_chip_id(fbm320_handle_t sensor, uint8_t* chipid)
{
    uint8_t data;
    esp_err_t ret = iot_fbm320_read_byte(sensor, FBM320_CHIP_ID_REG, &data);
    if (ret == ESP_FAIL) return ret;

    *chipid = data;
    return ESP_OK;
}

esp_err_t iot_fbm320_get_version_id(fbm320_handle_t sensor, uint8_t* data)
{
    uint8_t buf[2] = {0};
	uint8_t version = 0;

    esp_err_t ret = iot_fbm320_write_byte(sensor, FBM320_SOFTRESET_REG, FBM320_SOFTRESET_CMD);
    if (ret == ESP_FAIL) return ret;

    vTaskDelay(15 / portTICK_RATE_MS);

    ret = iot_fbm320_read_byte(sensor, FBM320_TAKE_MEAS_REG, buf);
    ret = iot_fbm320_read_byte(sensor, FBM320_VERSION_REG, buf);

    version = ((buf[0] & 0xC0) >> 6) | ((buf[1] & 0x70) >> 2);
    *data = version;
    return ESP_OK;
}

esp_err_t iot_fbm320_get_raw_temperature(fbm320_handle_t sensor, uint32_t* data)
{
    esp_err_t ret;

    ret = iot_fbm320_write_byte(sensor, FBM320_TAKE_MEAS_REG, FBM320_MEAS_TEMP);
    if (ret == ESP_FAIL) return ret;

    vTaskDelay(15 / portTICK_RATE_MS);

    uint8_t buf[3] = {0};
    ret = iot_fbm320_read(sensor, FBM320_READ_MEAS_REG_U, 3, buf);
    *data = ( buf[0] << 16 | buf[1] << 8 | buf[2]);

    return ESP_OK;
}  

esp_err_t iot_fbm320_get_raw_pressure(fbm320_handle_t sensor, uint32_t* data)
{   
    fbm320_dev_t* sens = (fbm320_dev_t*) sensor;
    
    esp_err_t ret = iot_fbm320_write_byte(sensor, FBM320_TAKE_MEAS_REG, sens->cmd_pressure);
    if (ret == ESP_FAIL) return ret;

    vTaskDelay(sens->time_pressure / portTICK_PERIOD_MS);

    uint8_t buf[3] = {0};
    ret = iot_fbm320_read(sensor, FBM320_READ_MEAS_REG_U, 3, buf);
    *data = ( buf[0] << 16 | buf[1] << 8 | buf[2]);

    return ESP_OK;
}

esp_err_t iot_fbm320_get_data(fbm320_handle_t sensor, fbm320_values_t* data_t)
{ 
    esp_err_t ret; 

    // Note: If any of the readings are out, try increasing the delay times.

    uint32_t temperature;
    ret = iot_fbm320_get_raw_temperature(sensor, &temperature);
    if (ret == ESP_FAIL) return ret;

    uint32_t pressure;
	ret = iot_fbm320_get_raw_pressure(sensor, &pressure);
    if (ret == ESP_FAIL) return ret;

    fbm320_dev_t* sens = (fbm320_dev_t*) sensor;
    sens->raw_temperature = temperature;
    sens->raw_pressure = pressure;

    iot_fbm320_calculate(sensor, data_t);

    return ESP_OK;
}

esp_err_t iot_fbm320_init(fbm320_handle_t sensor)
{
    esp_err_t ret;

    uint8_t chipid;
    ret = iot_fbm320_get_chip_id(sensor, &chipid);
    if (ret == ESP_FAIL || chipid != FBM320_CHIP_ID) 
    {
        return ESP_FAIL;
    }

    ret = iot_fbm320_read_calibration(sensor);
    if (ret == ESP_FAIL) return ret;

    iot_fbm320_set_oversampling_rate(sensor, FBM320_OVERSAMPLING_RATE_DEFAULT);

    return ESP_OK;
}

esp_err_t iot_fbm320_read_calibration(fbm320_handle_t sensor)
{
    esp_err_t ret;
    fbm320_dev_t* sens = (fbm320_dev_t*) sensor;
    uint16_t R[10] = {0};
	uint8_t tmp[FBM320_CALIBRATION_DATA_LENGTH] = {0};

    ret = iot_fbm320_read(sensor, FBM320_CALIBRATION_DATA_START0, FBM320_CALIBRATION_DATA_LENGTH - 2, tmp);
    if (ret == ESP_FAIL) return ret;

    ret = iot_fbm320_read_byte(sensor, FBM320_CALIBRATION_DATA_START1, (uint8_t *)tmp + 18);
    if (ret == ESP_FAIL) return ret;

    ret = iot_fbm320_read_byte(sensor, FBM320_CALIBRATION_DATA_START2, (uint8_t *)tmp + 19);
    if (ret == ESP_FAIL) return ret;

    /* Read OTP data */
	R[0] = (tmp[0] << 8 | tmp[1]);
	R[1] = (tmp[2] << 8 | tmp[3]);
	R[2] = (tmp[4] << 8 | tmp[5]);
	R[3] = (tmp[6] << 8 | tmp[7]);
	R[4] = (tmp[8] << 8 | tmp[9]);
	R[5] = (tmp[10] << 8 | tmp[11]);
	R[6] = (tmp[12] << 8 | tmp[13]);
	R[7] = (tmp[14] << 8 | tmp[15]);
	R[8] = (tmp[16] << 8 | tmp[17]);
	R[9] = (tmp[18] << 8 | tmp[19]);

    /* Coefficient reconstruction */
	sens->calibration_data.C0 = R[0] >> 4;
	sens->calibration_data.C1 = ((R[1] & 0xFF00) >> 5) | (R[2] & 7);
	sens->calibration_data.C2 = ((R[1] & 0xFF) << 1) | (R[4] & 1);
	sens->calibration_data.C3 = R[2] >> 3;
	sens->calibration_data.C5 = R[4] >> 1;
	sens->calibration_data.C6 = R[5] >> 3;
	sens->calibration_data.C8 = R[7] >> 3;
	sens->calibration_data.C9 = R[8] >> 2;
	sens->calibration_data.C10 = ((R[9] & 0xFF00) >> 6) | (R[8] & 3);
	sens->calibration_data.C11 = R[9] & 0xFF;

    /* The fbm320 sensor on the Azure IoT Kit is a hardware version b1 */
    sens->calibration_data.C4 = ((uint32_t)R[3] << 1) | (R[5] & 1);
	sens->calibration_data.C7 = ((uint32_t)R[6] << 2) | ((R[0] >> 2) & 3);
	sens->calibration_data.C12 = ((R[5] & 6) << 2) | (R[7] & 7);

    return ESP_OK;
}

void iot_fbm320_set_oversampling_rate(fbm320_handle_t sensor, fbm320_oversampling_rate rate)
{
    fbm320_dev_t* sens = (fbm320_dev_t*) sensor;
    sens->oversampling_rate = rate;

    switch(rate)
    {
        case FBM320_OSR_1024:
		sens->time_pressure = FBM320_CONVERSION_usTIME_OSR1024;
		sens->cmd_pressure = FBM320_MEAS_PRESS_OVERSAMP_0;
		break;

	case FBM320_OSR_2048:
		sens->time_pressure = FBM320_CONVERSION_usTIME_OSR2048;
		sens->cmd_pressure = FBM320_MEAS_PRESS_OVERSAMP_1;
		break;

	case FBM320_OSR_4096:
		sens->time_pressure = FBM320_CONVERSION_usTIME_OSR4096;
		sens->cmd_pressure = FBM320_MEAS_PRESS_OVERSAMP_2;
		break;

	case FBM320_OSR_8192:
		sens->time_pressure = FBM320_CONVERSION_usTIME_OSR8192;
		sens->cmd_pressure = FBM320_MEAS_PRESS_OVERSAMP_3;
		break;
    }
}

void iot_fbm320_calculate(fbm320_handle_t sensor, fbm320_values_t* data_t)
{
	int32_t X01, X02, X03, X11, X12, X13, X21, X22, X23, X24, X25, X26, X31, X32;
	int32_t PP1, PP2, PP3, PP4, CF;
	int32_t RT, RP, UT, UP, DT, DT2;

    fbm320_dev_t* sens = (fbm320_dev_t*) sensor;

    // Calculate temperature.
    UT = sens->raw_temperature;
    DT = ((UT - 8388608) >> 4) + (sens->calibration_data.C0 << 4);
    X01 = (sens->calibration_data.C1 + 4418) * DT >> 1; // hardware version b1
    X02 = ((((sens->calibration_data.C2 - 256) * DT) >> 14) * DT) >> 4;
	X03 = (((((sens->calibration_data.C3 * DT) >> 18) * DT) >> 18) * DT);
	RT =  ((2500 << 15) - X01 - X02 - X03) >> 15;

    data_t->temperature = RT * FBM320_TEMP_RESOLUTION;

    // Calculate pressure. 
    // Hardware version b1 - X11, X13, X21, X24, X25, X26
    DT2 = (X01 + X02 + X03) >> 12;
    UP = sens->raw_pressure;
    X11 = (sens->calibration_data.C5 * DT2); 
    X12 = (((sens->calibration_data.C6 * DT2) >> 16) * DT2) >> 2;
    X13 = ((X11 + X12) >> 10) + ((sens->calibration_data.C4 + 211288) << 4); 
	X21 = ((sens->calibration_data.C8 + 7209) * DT2) >> 10; 
    X22 = (((sens->calibration_data.C9 * DT2) >> 17) * DT2) >> 12;
	X23 = (X22 >= X21) ? (X22 - X21) : (X21 - X22);
   
    X24 = (X23 >> 11) * (sens->calibration_data.C7 + 285594);
	X25 = ((X23 & 0x7FF) * (sens->calibration_data.C7 + 285594)) >> 11;
    if ((X22 - X21) < 0)
        X26 = ((0 - X24 - X25) >> 11) + sens->calibration_data.C7 + 285594;
    else
        X26 = ((X24 + X25) >> 11) + sens->calibration_data.C7 + 285594;

    PP1 = ((UP - 8388608) - X13) >> 3;
	PP2 = (X26 >> 11) * PP1;
	PP3 = ((X26 & 0x7FF) * PP1) >> 11;
	PP4 = (PP2 + PP3) >> 10;
	CF = (2097152 + sens->calibration_data.C12 * DT2) >> 3;
	X31 = (((CF * sens->calibration_data.C10) >> 17) * PP4) >> 2;
	X32 = (((((CF * sens->calibration_data.C11) >> 15) * PP4) >> 18) * PP4);
	RP = ((X31 + X32) >> 15) + PP4 + 100000;

    data_t->pressure = RP;
    data_t->altitude = iot_fbm320_pressure_to_altitude(RP);
}

int32_t iot_fbm320_pressure_to_altitude(int32_t pressure)
{
	int32_t RP, h0, hs0, HP1, HP2, RH;
	int32_t hs1, dP0;
	int8_t P0;
	
	RP = pressure * 8;

	if ( RP >= 824000 ) {
		P0	=	103	;
		h0	=	-138507	;
		hs0	=	-5252	;
		hs1	=	311	;
	} else if ( RP >= 784000 ) {
		P0	=	98	;
		h0	=	280531	;
		hs0	=	-5468	;
		hs1	=	338	;
	} else if ( RP >= 744000 ) {
		P0	=	93	;
		h0	=	717253	;
		hs0	=	-5704	;
		hs1	=	370	;
	} else if ( RP >= 704000 ) {
		P0	=	88	;
		h0	=	1173421	;
		hs0	=	-5964	;
		hs1	=	407	;
	} else if ( RP >= 664000 ) {
		P0	=	83	;
		h0	=	1651084	;
		hs0	=	-6252	;
		hs1	=	450	;
	} else if ( RP >= 624000 ) {
		P0	=	78	;
		h0	=	2152645	;
		hs0	=	-6573	;
		hs1	=	501	;
	} else if ( RP >= 584000 ) {
		P0	=	73	;
		h0	=	2680954	;
		hs0	=	-6934	;
		hs1	=	560	;
	} else if ( RP >= 544000 ) {
		P0	=	68	;
		h0	=	3239426	;
		hs0	=	-7342	;
		hs1	=	632	;
	} else if ( RP >= 504000 ) {
		P0	=	63	;
		h0	=	3832204	;
		hs0	=	-7808	;
		hs1	=	719	;
	} else if ( RP >= 464000 ) {
		P0	=	58	;
		h0	=	4464387	;
		hs0	=	-8345	;
		hs1	=	826	;
	} else if ( RP >= 424000 ) {
		P0	=	53	;
		h0	=	5142359	;
		hs0	=	-8972	;
		hs1	=	960	;
	} else if ( RP >= 384000 ) {
		P0	=	48	;
		h0	=	5874268	;
		hs0	=	-9714	;
		hs1	=	1131	;
	} else if ( RP >= 344000 ) {
		P0	=	43	;
		h0	=	6670762	;
		hs0	=	-10609	;
		hs1	=	1354	;
	} else if ( RP >= 304000 ) {
		P0	=	38	;
		h0	=	7546157	;
		hs0	=	-11711	;
		hs1	=	1654	;
	} else if ( RP >= 264000 ) {
		P0	=	33	;
		h0	=	8520395	;
		hs0	=	-13103	;
		hs1	=	2072	;
	} else {
		P0	=	28	;
		h0	=	9622536	;
		hs0	=	-14926	;
		hs1	=	2682	;
	}

	dP0	=	RP - P0 * 8000;
	HP1	=	( hs0 * dP0 ) >> 1;
	HP2	=	((( hs1 * dP0 ) >> 14 ) * dP0 ) >> 4;
	RH	=	(( HP1 + HP2 ) >> 8 ) + h0;

	return RH;
}
