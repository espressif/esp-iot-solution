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

#ifndef _APDS9960_H_
#define _APDS9960_H_

#include "driver/i2c.h"
#include "i2c_bus.h"
#include "esp_log.h"
#include "math.h"

#define APDS9960_I2C_ADDRESS    (0x39)
#define WRITE_BIT               I2C_MASTER_WRITE  /*!< I2C master write */
#define READ_BIT                I2C_MASTER_READ   /*!< I2C master read */
#define ACK_CHECK_EN            0x1               /*!< I2C master will check ack from slave*/
#define ACK_CHECK_DIS           0x0               /*!< I2C master will not check ack from slave */
#define ACK_VAL                 0x0               /*!< I2C ack value */
#define NACK_VAL                0x1               /*!< I2C nack value */
#define APDS9960_TIME_MULT      2.78              //millisec
#define ERROR                   0xFF

#define APDS9960_UP             0x01
#define APDS9960_DOWN           0x02
#define APDS9960_LEFT           0x03
#define APDS9960_RIGHT          0x04

/* Gesture parameters */
#define GESTURE_THRESHOLD_OUT   10   //Output threshold
#define GESTURE_SENSITIVITY_1   50   //sensitivity 1
#define GESTURE_SENSITIVITY_2   20   //distance sensitivity, increase the value to get more sensitive

#define APDS9960_WHO_AM_I_REG   ((uint8_t)0x92)
#define APDS9960_WHO_AM_I_VAL   ((uint8_t)0xAB)
#define APDS9960_MODE_ENABLE    0x80
#define APDS9960_GEN_BIT        (APDS9960_BIT(6))
#define APDS9960_PIEN_BIT       (APDS9960_BIT(5))
#define APDS9960_AIEN_BIT       (APDS9960_BIT(4))
#define APDS9960_WEN_BIT        (APDS9960_BIT(3))
#define APDS9960_PEN_BIT        (APDS9960_BIT(2))
#define APDS9960_AEN_BIT        (APDS9960_BIT(1))
#define APDS9960_PON_BIT        (APDS9960_BIT(0))
#define APDS9960_GEN_MASK       ((uint8_t)0x40)
#define APDS9960_PIEN_MASK      ((uint8_t)0x20)
#define APDS9960_AIEN_MASK      ((uint8_t)0x10)
#define APDS9960_WEN_MASK       ((uint8_t)0x80)
#define APDS9960_PEN_MASK       ((uint8_t)0x04)
#define APDS9960_AEN_MASK       ((uint8_t)0x02)
#define APDS9960_PON_MASK       ((uint8_t)0x01)

#define APDS9960_ATIME          0x81
#define APDS9960_WTIME          0x83
#define APDS9960_AILTL          0x84
#define APDS9960_AILTH          0x85
#define APDS9960_AIHTL          0x86
#define APDS9960_AIHTH          0x87
#define APDS9960_PILT           0x89
#define APDS9960_PIHT           0x8B
#define APDS9960_PERS           0x8C
#define APDS9960_CONFIG1        0x8D
#define APDS9960_PPULSE         0x8E
#define APDS9960_CONTROL        0x8F
#define APDS9960_CONFIG2        0x90
#define APDS9960_ID             0x92
#define APDS9960_STATUS         0x93
#define APDS9960_CDATAL         0x94
#define APDS9960_CDATAH         0x95
#define APDS9960_RDATAL         0x96
#define APDS9960_RDATAH         0x97
#define APDS9960_GDATAL         0x98
#define APDS9960_GDATAH         0x99
#define APDS9960_BDATAL         0x9A
#define APDS9960_BDATAH         0x9B
#define APDS9960_PDATA          0x9C
#define APDS9960_POFFSET_UR     0x9D
#define APDS9960_POFFSET_DL     0x9E
#define APDS9960_CONFIG3        0x9F
#define APDS9960_GPENTH         0xA0
#define APDS9960_GEXTH          0xA1
#define APDS9960_GCONF1         0xA2
#define APDS9960_GCONF2         0xA3
#define APDS9960_GOFFSET_U      0xA4
#define APDS9960_GOFFSET_D      0xA5
#define APDS9960_GOFFSET_L      0xA7
#define APDS9960_GOFFSET_R      0xA9
#define APDS9960_GPULSE         0xA6
#define APDS9960_GCONF3         0xAA
#define APDS9960_GCONF4         0xAB
#define APDS9960_GFLVL          0xAE
#define APDS9960_GSTATUS        0xAF
#define APDS9960_IFORCE         0xE4
#define APDS9960_PICLEAR        0xE5
#define APDS9960_CICLEAR        0xE6
#define APDS9960_AICLEAR        0xE7
#define APDS9960_GFIFO_U        0xFC
#define APDS9960_GFIFO_D        0xFD
#define APDS9960_GFIFO_L        0xFE
#define APDS9960_GFIFO_R        0xFF

/* ALS Gain (AGAIN) values */
typedef enum {
    APDS9960_AGAIN_1X   = 0x00, /**<  1x gain  */
    APDS9960_AGAIN_4X   = 0x01, /**<  2x gain  */
    APDS9960_AGAIN_16X  = 0x02, /**<  16x gain */
    APDS9960_AGAIN_64X  = 0x03  /**<  64x gain */
} apds9960_again_t;

/* Proximity Gain (PGAIN) values */
typedef enum {
    APDS9960_PGAIN_1X = 0x00, /**<  1x gain  */
    APDS9960_PGAIN_2X = 0x01, /**<  2x gain  */
    APDS9960_PGAIN_4X = 0x02, /**<  4x gain  */
    APDS9960_PGAIN_8X = 0x03  /**<  8x gain  */
} apds9960_pgain_t;

/* Gesture Gain (PGAIN) values */
typedef enum {
    APDS9960_GGAIN_1X = 0x00, /**<  1x gain  */
    APDS9960_GGAIN_2X = 0x01, /**<  2x gain  */
    APDS9960_GGAIN_4X = 0x02, /**<  4x gain  */
    APDS9960_GGAIN_8X = 0x03, /**<  8x gain  */
} apds9960_ggain_t;

typedef enum {
    APDS9960_PPULSELEN_4US  = 0x00, /**<  4uS   */
    APDS9960_PPULSELEN_8US  = 0x01, /**<  8uS   */
    APDS9960_PPULSELEN_16US = 0x02, /**<  16uS  */
    APDS9960_PPULSELEN_32US = 0x03  /**<  32uS  */
} apds9960_ppulse_len_t;

typedef enum {
    APDS9960_LEDDRIVE_100MA = 0x00, /**<  100mA   */
    APDS9960_LEDDRIVE_50MA  = 0x01, /**<  50mA    */
    APDS9960_LEDDRIVE_25MA  = 0x02, /**<  25mA    */
    APDS9960_LEDDRIVE_12MA  = 0x03  /**<  12.5mA  */
} apds9960_leddrive_t;

typedef enum {
    APDS9960_LEDBOOST_100PCNT = 0x00, /**<  100%   */
    APDS9960_LEDBOOST_150PCNT = 0x01, /**<  150%   */
    APDS9960_LEDBOOST_200PCNT = 0x02, /**<  200%   */
    APDS9960_LEDBOOST_300PCNT = 0x03  /**<  300%   */
} apds9960_ledboost_t;

typedef enum {
    APDS9960_DIMENSIONS_ALL         = 0x00,
    APDS9960_DIMENSIONS_UP_DOWM     = 0x01,
    APGS9960_DIMENSIONS_LEFT_RIGHT  = 0x02,
} apds9960_dimensions_t;

typedef enum {
    APDS9960_GFIFO_1    = 0x00,
    APDS9960_GFIFO_4    = 0x01,
    APDS9960_GFIFO_8    = 0x02,
    APDS9960_GFIFO_16   = 0x03,
} apds9960_gfifo_t;

/*set the number of pulses to be output on the LDR pin.*/
typedef enum {
    APDS9960_GPULSELEN_4US  = 0x00,
    APDS9960_GPULSELEN_8US  = 0x01,
    APDS9960_GPULSELEN_16US = 0x02,
    APDS9960_GPULSELEN_32US = 0x03,
} apds9960_gpulselen_t;

/* Gesture wait time values */
typedef enum {
    APDS9960_GWTIME_0MS     = 0,
    APDS9960_GWTIME_2_8MS   = 1,
    APDS9960_GWTIME_5_6MS   = 2,
    APDS9960_GWTIME_8_4MS   = 3,
    APDS9960_GWTIME_14_0MS  = 4,
    APDS9960_GWTIME_22_4MS  = 5,
    APDS9960_GWTIME_30_8MS  = 6,
    APDS9960_GWTIME_39_2MS  = 7
} apds9960_gwtime_t;

/* Acceptable parameters for setMode */
typedef enum {
    APDS9960_POWER              = 0,
    APDS9960_AMBIENT_LIGHT      = 1,
    APDS9960_PROXIMITY          = 2,
    APDS9960_WAIT               = 3,
    APDS9960_AMBIENT_LIGHT_INT  = 4,
    APDS9960_PROXIMITY_INT      = 5,
    APDS9960_GESTURE            = 6,
    APDS9960_ALL                = 7,
} apds9960_mode_t;

#define DEFAULT_ATIME           219     // 103ms
#define DEFAULT_WTIME           246     // 27ms
#define DEFAULT_PROX_PPULSE     0x87    // 16us, 8 pulses
#define DEFAULT_GESTURE_PPULSE  0x89    // 16us, 10 pulses
#define DEFAULT_POFFSET_UR      0       // 0 offset
#define DEFAULT_POFFSET_DL      0       // 0 offset      
#define DEFAULT_CONFIG1         0x60    // No 12x wait (WTIME) factor
#define DEFAULT_LDRIVE          LED_DRIVE_100MA
#define DEFAULT_PGAIN           PGAIN_4X
#define DEFAULT_AGAIN           APDS9960_AGAIN_4X
#define DEFAULT_PILT            0       // Low proximity threshold
#define DEFAULT_PIHT            50      // High proximity threshold
#define DEFAULT_AILT            0xFFFF  // Force interrupt for calibration
#define DEFAULT_AIHT            0
#define DEFAULT_PERS            0x11    // 2 consecutive prox or ALS for int.
#define DEFAULT_CONFIG2         0x01    // No saturation interrupts or LED boost  
#define DEFAULT_CONFIG3         0       // Enable all photodiodes, no SAI
#define DEFAULT_GPENTH          40      // Threshold for entering gesture mode
#define DEFAULT_GEXTH           30      // Threshold for exiting gesture mode    
#define DEFAULT_GCONF1          0x40    // 4 gesture events for int., 1 for exit
#define DEFAULT_GGAIN           GGAIN_4X
#define DEFAULT_GLDRIVE         LED_DRIVE_100MA
#define DEFAULT_GWTIME          APDS9960_GWTIME_2_8MS
#define DEFAULT_GOFFSET         0       // No offset scaling for gesture mode
#define DEFAULT_GPULSE          0xC9    // 32us, 10 pulses
#define DEFAULT_GCONF3          0       // All photodiodes active during gesture
#define DEFAULT_GIEN            0       // Disable gesture interrupts

typedef struct control {
    uint8_t again : 2; //ALS and Color gain control
    uint8_t pgain : 2; //proximity gain control
    uint8_t leddrive : 2; //led drive strength
} apds9960_control_t;

typedef struct pers {
    //ALS Interrupt Persistence. Controls rate of Clear channel interrupt to the host processor
    uint8_t apers : 4;
    //proximity interrupt persistence, controls rate of prox interrupt to host processor
    uint8_t ppers : 4;
} apds9960_pers_t;

typedef struct config1 {
    uint8_t wlong : 1;
} apds9960_config1_t;

typedef struct config2 {
    /* Additional LDR current during proximity and gesture LED pulses. Current value, set by LDRIVE,
     is increased by the percentage of LED_BOOST.*/
    uint8_t led_boost : 2;
    //clear photodiode saturation int enable
    uint8_t cpsien : 1;
    //proximity saturation interrupt enable
    uint8_t psien : 1;
} apds9960_config2_t;

typedef struct config3 {
    //proximity mask
    uint8_t pmask_r : 1;
    uint8_t pmask_l : 1;
    uint8_t pmask_d : 1;
    uint8_t pmask_u : 1;
    /* Sleep After Interrupt. When enabled, the device will automatically enter low power mode
     when the INT pin is asserted and the state machine has progressed to the SAI decision block.
     Normal operation is resumed when INT pin is cleared over I2C.*/
    uint8_t sai : 1;
    /* Proximity Gain Compensation Enable. This bit provides gain compensation when proximity
     photodiode signal is reduced as a result of sensor masking. If only one diode of the diode pair
     is contributing, then only half of the signal is available at the ADC; this results in a maximum
     ADC value of 127. Enabling PCMP enables an additional gain of 2X, resulting in a maximum
     ADC value of 255.*/
    uint8_t pcmp : 1;
} apds9960_config3_t;

typedef struct gconf1 {
    /* Gesture Exit Persistence. When a number of consecutive gesture end occurrences become
     * equal or greater to the GEPERS value, the Gesture state machine is exited.*/
    uint8_t gexpers : 2;
    /* Gesture Exit Mask. Controls which of the gesture detector photodiodes (UDLR) will be included
     * to determine a gesture end and subsequent exit of the gesture state machine. Unmasked
     * UDLR data will be compared with the value in GTHR_OUT. Field value bits correspond to UDLR
     * detectors.*/
    uint8_t gexmsk : 4;
    /* Gesture FIFO Threshold. This value is compared with the FIFO Level (i.e. the number of UDLR
     * datasets) to generate an interrupt (if enabled).*/
    uint8_t gfifoth : 2;
} apds9960_gconf1_t;

typedef struct gconf2 {
    /* Gesture Wait Time. The GWTIME controls the amount of time in a low power mode
     * between gesture detection cycles.*/
    uint8_t gwtime : 3;
    uint8_t gldrive : 2; //Gesture LED Drive Strength. Sets LED Drive Strength in gesture mode.
    uint8_t ggain : 2; //Gesture Gain Control. Sets the gain of the proximity receiver in gesture mode.
} apds9960_gconf2_t;

typedef struct gconf3 {
    /* Gesture Dimension Select. Selects which gesture photodiode pairs are enabled to gather
     * results during gesture. */
    uint8_t gdims : 2;
} apds9960_gconf3_t;

typedef struct gconf4 {
    /* Gesture Mode. Reading this bit reports if the gesture state machine is actively running, 1
     * = Gesture, 0= ALS, Proximity, Color. Writing a 1 to this bit causes immediate entry in to the
     * gesture state machine (as if GPENTH had been exceeded). Writing a 0 to this bit causes exit of
     * gesture when current analog conversion has finished (as if GEXTH had been exceeded).*/
    uint8_t gmode : 1;
    /* Gesture interrupt enable. Gesture Interrupt Enable. When asserted, all gesture related
     * interrupts are unmasked.*/
    uint8_t gien : 2;
} apds9960_gconf4_t;

typedef struct enable {
    uint8_t pon : 1; //power on
    uint8_t aen : 1; //ALS enable
    uint8_t pen : 1; //Proximity detect enable
    uint8_t wen : 1; //wait timer enable
    uint8_t aien : 1; //ALS interrupt enable
    uint8_t pien : 1; //proximity interrupt enable
    uint8_t gen : 1; //gesture enable
} apds9960_enable_t;

typedef struct status {
    /* ALS Valid. Indicates that an ALS cycle has completed since AEN was asserted or since a read
     from any of the ALS/Color data registers.*/
    uint8_t avalid : 1;
    /* Proximity Valid. Indicates that a proximity cycle has completed since PEN was asserted or since
     PDATA was last read. A read of PDATA automatically clears PVALID.*/
    uint8_t pvalid : 1;
    /* Gesture Interrupt. GINT is asserted when GFVLV becomes greater than GFIFOTH or if GVALID
     has become asserted when GMODE transitioned to zero. The bit is reset when FIFO is
     completely emptied (read).  */
    uint8_t gint : 1;
    //ALS Interrupt. This bit triggers an interrupt if AIEN in ENABLE is set.
    uint8_t aint : 1;
    //Proximity Interrupt. This bit triggers an interrupt if PIEN in ENABLE is set.
    uint8_t pint : 1;
    /* Indicates that an analog saturation event occurred during a previous proximity or gesture
     cycle. Once set, this bit remains set until cleared by clear proximity interrupt special function
     command (0xE5 PICLEAR) or by disabling Prox (PEN=0). This bit triggers an interrupt if PSIEN
     is set.  */
    uint8_t pgsat : 1;
    /* Clear Photodiode Saturation. When asserted, the analog sensor was at the upper end of its
     dynamic range. The bit can be de-asserted by sending a Clear channel interrupt command
     (0xE6 CICLEAR) or by disabling the ADC (AEN=0). This bit triggers an interrupt if CPSIEN is set. */
    uint8_t cpsat : 1;
} apds9960_status_t;

typedef struct gstatus {
    /* Gesture FIFO Data. GVALID bit is sent when GFLVL becomes greater than GFIFOTH (i.e. FIFO has
     enough data to set GINT). GFIFOD is reset when GMODE = 0 and the GFLVL=0 (i.e. All FIFO data
     has been read). */
    uint8_t gvalid : 1;
    /* Gesture FIFO Overflow. A setting of 1 indicates that the FIFO has filled to capacity and that new
     gesture detector data has been lost. */
    uint8_t gfov : 1;
} apds9960_gstatus_t;

typedef struct ppulse {
    /*Proximity Pulse Count. Specifies the number of proximity pulses to be generated on LDR.
     Number of pulses is set by PPULSE value plus 1. */
    uint8_t ppulse : 6;
    //Proximity Pulse Length. Sets the LED-ON pulse width during a proximity LDR pulse.
    uint8_t pplen : 2;
} apds9960_propulse_t;

typedef struct gpulse {
    /* Number of Gesture Pulses. Specifies the number of pulses to be generated on LDR.
     Number of pulses is set by GPULSE value plus 1. */
    uint8_t gpulse : 6;
    //Gesture Pulse Length. Sets the LED_ON pulse width during a Gesture LDR Pulse.
    uint8_t gplen : 2;
} apds9960_gespulse_t;

typedef void *apds9960_handle_t;

#ifdef __cplusplus
extern "C"
{
#endif

/**
 * @brief Create and init sensor object and return a sensor handle
 *
 * @param bus I2C bus object handle
 * @param dev_addr I2C device address of sensor
 *
 * @return
 *     - NULL Fail
 *     - Others Success
 */
apds9960_handle_t apds9960_create(i2c_bus_handle_t bus,
                                      uint8_t dev_addr);

/**
 * @brief Delete and release a sensor object
 *
 * @param sensor Point to object handle of apds9960
 *
 * @return
 *     - ESP_OK Success
 *     - ESP_FAIL Fail
 */
esp_err_t apds9960_delete(apds9960_handle_t *sensor);

/**
 * @brief Set APDS9960 sensor timeout for I2C operations

 * @param sensor object handle of apds9960
 * @param tout_ms timeout value, in millisecond

 * @return
 *     - ESP_OK Success
 *     - ESP_FAIL Fail
 */
esp_err_t apds9960_set_timeout(apds9960_handle_t sensor, uint32_t tout_ms);

/**
 * @brief init sensor for gesture
 *
 * @return
 *     - ESP_OK Success
 *     - ESP_FAIL Fail
 */
esp_err_t apds9960_gesture_init(apds9960_handle_t sensor);

/**
 * @brief Get device identification of APDS9960
 *
 * @param sensor object handle of apds9960
 * @param deviceid a pointer of device ID
 *
 * @return
 *     - ESP_OK Success
 *     - ESP_FAIL Fail
 */
esp_err_t apds9960_get_deviceid(apds9960_handle_t sensor, uint8_t *deviceid);

/**
 * @brief Configure work mode
 *
 * @param sensor object handle of apds9960
 * @param mode one of apds9960_mode_t struct
 *
 * @return
 *     - ESP_OK Success
 *     - ESP_FAIL Fail
 */
esp_err_t apds9960_set_mode(apds9960_handle_t sensor, apds9960_mode_t mode);

/**
 * @brief Get work mode
 *
 * @param sensor object handle of apds9960
 *
 * @return
 *     - apds9960_mode_t
 */
apds9960_mode_t apds9960_get_mode(apds9960_handle_t sensor);

/**
 * @brief Set wait time
 * If multiple engines are enabled, then the operational flow progresses in the following order:
 * idle, proximity, gesture, wait,color/ALS, and sleep.
 *
 * @param sensor object handle of apds9960
 * @param time wait time
 *
 * @return
 *     - ESP_OK Success
 *     - ESP_FAIL Fail
 */
esp_err_t apds9960_set_wait_time(apds9960_handle_t sensor, uint8_t time);

/**
 * @brief Sets the integration time for the ADC of the APDS9960, in millis
 *
 * @param sensor object handle of apds9960
 * @param iTimeMS the integration time for the ADC of the APDS9960
 *
 * @return
 *     - ESP_OK Success
 *     - ESP_FAIL Fail
 */
esp_err_t apds9960_set_adc_integration_time(apds9960_handle_t sensor,
        uint16_t iTimeMS);

/**
 * @brief Get the integration time for the ADC of the APDS9960, in millis
 *
 * @param sensor object handle of apds9960
 *
 * @return
 *     - iTimeMS the integration time for the ADC of the APDS9960
 */
float apds9960_get_adc_integration_time(apds9960_handle_t sensor);

/**
 * @brief Set the color/ALS gain on the APDS9960 (adjusts the sensitivity to light)
 *
 * @param sensor object handle of apds9960
 * @param aGain the color/ALS gain on the APDS9960
 *
 * @return
 *     - ESP_OK Success
 *     - ESP_FAIL Fail
 */
esp_err_t apds9960_set_ambient_light_gain(apds9960_handle_t sensor,
        apds9960_again_t aGain);

/**
 * @brief Get the color/ALS gain on the APDS9960 (adjusts the sensitivity to light)
 *
 * @param sensor object handle of apds9960
 *
 * @return
 *     - the color/ALS gain on the APDS9960
 */
apds9960_again_t apds9960_get_ambient_light_gain(apds9960_handle_t sensor);

/**
 * @brief Sets the LED current boost value
 * Sets the LED drive strength for proximity and ALS
 *
 * @param sensor object handle of apds9960
 * @param drive the value (0-3) for the LED drive strength
 * @param boost the value (0-3) for current boost (100-300%)
 *
 * @return
 *     - ESP_OK Success
 *     - ESP_FAIL Fail
 */
esp_err_t apds9960_set_led_drive_boost(apds9960_handle_t sensor,
        apds9960_leddrive_t drive, apds9960_ledboost_t boost);

/**
 * @brief  Enable proximity engine on APDS9960
 *
 * @param sensor object handle of apds9960
 * @param en true to enable proximity engine, false to disable proximity engine
 *
 * @return
 *     - ESP_OK Success
 *     - ESP_FAIL Fail
 */
esp_err_t apds9960_enable_proximity_engine(apds9960_handle_t sensor, bool en);

/**
 * @brief  Set the receiver gain for proximity detection
 *
 * @param sensor object handle of apds9960
 * @param pGain the value (apds9960_pgain_t) for the gain
 *
 * @return
 *     - ESP_OK Success
 *     - ESP_FAIL Fail
 */
esp_err_t apds9960_set_proximity_gain(apds9960_handle_t sensor,
        apds9960_pgain_t pGain);

/**
 * @brief  Gets the receiver gain for proximity detection
 *
 * @param sensor object handle of apds9960
 *
 * @return
 *     - the receiver gain for proximity detection
 */
apds9960_pgain_t apds9960_get_proximity_gain(apds9960_handle_t sensor);

/**
 * @brief  Set proximity pulse count and length
 *
 * @param sensor object handle of apds9960
 * @param pLen proximity pulse count
 * @param pulses proximity pulse length
 *
 * @return
 *     - ESP_OK Success
 *     - ESP_FAIL Fail
 */
esp_err_t apds9960_set_proximity_pulse(apds9960_handle_t sensor,
        apds9960_ppulse_len_t pLen, uint8_t pulses);

/**
 * @brief Turns proximity interrupts on or off
 *
 * @param sensor object handle of apds9960
 * @param en true to enable interrupts, false to disable interrupts
 *
 * @return
 *     - ESP_OK Success
 *     - ESP_FAIL Fail
 */
esp_err_t apds9960_enable_proximity_interrupt(apds9960_handle_t sensor, bool en);

/**
 * @brief Get proximity interrupts status
 *
 * @param sensor object handle of apds9960
 *
 * @return
 *     - true means valid
 *     - false means error
 */
bool apds9960_get_proximity_interrupt(apds9960_handle_t sensor);

/**
 * @brief Read proximity data
 *
 * @param sensor object handle of apds9960
 *
 * @return
 *     - the value of proximity data
 */
uint8_t apds9960_read_proximity(apds9960_handle_t sensor);

/**
 * @brief Sets the lower threshold for proximity detection
 *
 * @param sensor object handle of apds9960
 * @param low the lower proximity threshold
 * @param high the higher proximity threshold
 * @param persistance count
 *
 * @return
 *     - ESP_OK Success
 *     - ESP_FAIL Fail
 */
esp_err_t apds9960_set_proximity_interrupt_threshold(apds9960_handle_t sensor,
        uint8_t low, uint8_t high, uint8_t persistance);

/**
 * @brief Enable gesture engine on APDS9960
 *
 * @param sensor object handle of apds9960
 * @param en true to enable gesture engine, false to disable gesture engine
 *
 * @return
 *     - ESP_OK Success
 *     - ESP_FAIL Fail
 */
esp_err_t apds9960_enable_gesture_engine(apds9960_handle_t sensor, bool en);

/**
 * @brief Get gesture status
 *
 * @param sensor object handle of apds9960
 *
 * @return
 *     - true means valid
 *     - false means error
 */
bool apds9960_gesture_valid(apds9960_handle_t sensor);

/**
 * @brief Set gesture dimensions
 *
 * @param sensor object handle of apds9960
 * @param dims gesture dimensions
 *
 * @return
 *     - ESP_OK Success
 *     - ESP_FAIL Fail
 */
esp_err_t apds9960_set_gesture_dimensions(apds9960_handle_t sensor,
        uint8_t dims);

/**
 * @brief Sets the FIFO threshold for gesture
 *
 * @param sensor object handle of apds9960
 * @param thresh the FIFO threshold for gesture
 *
 * @return
 *     - ESP_OK Success
 *     - ESP_FAIL Fail
 */
esp_err_t apds9960_set_gesture_fifo_threshold(apds9960_handle_t sensor,
        uint8_t thresh);

/**
 * @brief  Set the gesture gain for gesture detection
 *
 * @param sensor object handle of apds9960
 * @param gGain the value (apds9960_ggain_t) for the gain
 *
 * @return
 *     - ESP_OK Success
 *     - ESP_FAIL Fail
 */
esp_err_t apds9960_set_gesture_gain(apds9960_handle_t sensor, apds9960_ggain_t gGain);

/**
 * @brief Sets the proximity threshold for gesture
 *
 * @param sensor object handle of apds9960
 * @param entthresh the proximity enter threshold for gesture
 * @param exitthresh the proximity exit threshold for gesture
 *
 * @return
 *     - ESP_OK Success
 *     - ESP_FAIL Fail
 */
esp_err_t apds9960_set_gesture_proximity_threshold(apds9960_handle_t sensor,
        uint8_t entthresh, uint8_t exitthresh);

/**
 * @brief Sets the offset for gesture
 *
 * @param sensor object handle of apds9960
 * @param offset_up the U offset for gesture
 * @param offset_down the D offset for gesture
 * @param offset_left the L offset for gesture
 * @param offset_right the R offset for gesture
 *
 * @return
 *     - ESP_OK Success
 *     - ESP_FAIL Fail
 */
esp_err_t apds9960_set_gesture_offset(apds9960_handle_t sensor,
        uint8_t offset_up, uint8_t offset_down, uint8_t offset_left,
        uint8_t offset_right);

/**
 * @brief Processes a gesture event and returns best guessed gesture
 *
 * @param sensor object handle of apds9960
 *
 * @return
 *     - Number corresponding to gesture. -1 on error.
 */
uint8_t apds9960_read_gesture(apds9960_handle_t sensor);

/**
 * @brief Reset some temp counts of gesture detection
 *
 * @param sensor object handle of apds9960
 *
 * @return
 *     - NULL
 */
void apds9960_reset_counts(apds9960_handle_t sensor);

/**
 * @brief  Set gesture pulse count and length
 *
 * @param sensor object handle of apds9960
 * @param gpulseLen Number OF PULSES
 *      FIELD                   VALUE PULSE LENGTH
 *  APDS9960_GPULSELEN_4US          4 μs
 *  APDS9960_GPULSELEN_8US          8 μs
 *  APDS9960_GPULSELEN_16US        16 μs
 *  APDS9960_GPULSELEN_32US        32 μs
 * @param pulses PULSE LENGTH
 *          FIELD               VALUE Number OF PULSES
 *  APDS9960_GPULSELEN_4US          = 0x00,
 *  APDS9960_GPULSELEN_8US          = 0x01,
 *  APDS9960_GPULSELEN_16US         = 0x02,
 *  APDS9960_GPULSELEN_32US         = 0x03,
 *
 * @return
 *     - ESP_OK Success
 *     - ESP_FAIL Fail
 */
esp_err_t apds9960_set_gesture_pulse(apds9960_handle_t sensor,
        apds9960_gpulselen_t gpulseLen, uint8_t pulses);

/**
 * @brief Turns gesture-related interrupts on or off
 *
 * @param sensor object handle of apds9960
 * @param en true to enable interrupts, false to disable interrupts
 *
 * @return
 *     - ESP_OK Success
 *     - ESP_FAIL Fail
 */
esp_err_t apds9960_enable_gesture_interrupt(apds9960_handle_t sensor, bool en);

/**
 * @brief Sets the time in low power mode between gesture detections
 *
 * @param sensor object handle of apds9960
 * @param time the value for the wait time
 *          Value                   Wait time
 *   APDS9960_GWTIME_0MS            0 ms
 *   APDS9960_GWTIME_2_8MS          2.8 ms
 *   APDS9960_GWTIME_5_6MS          5.6 ms
 *   APDS9960_GWTIME_8_4MS          8.4 ms
 *   APDS9960_GWTIME_14_0MS         14.0 ms
 *   APDS9960_GWTIME_22_4MS         22.4 ms
 *   APDS9960_GWTIME_30_8MS         30.8 ms
 *   APDS9960_GWTIME_39_2MS         39.2 ms
 *
 * @return
 *     - ESP_OK Success
 *     - ESP_FAIL Fail
 */
esp_err_t apds9960_set_gesture_waittime(apds9960_handle_t sensor, apds9960_gwtime_t time);

/**
 * @brief  Enable color engine on APDS9960
 *
 * @param sensor object handle of apds9960
 * @param en true to enable interrupts, false to disable interrupts
 *
 * @return
 *     - ESP_OK Success
 *     - ESP_FAIL Fail
 */
esp_err_t apds9960_enable_color_engine(apds9960_handle_t sensor, bool en);

/**
 * @brief  Get color detection status
 *
 * @param sensor object handle of apds9960
 *
 * @return
 *     - true Success
 *     - false Fail
 */
bool apds9960_color_data_ready(apds9960_handle_t sensor);

/**
 * @brief  Reads the raw red, green, blue and clear channel values
 *
 * @param sensor object handle of apds9960
 * @param r pointer of r
 * @param g pointer of g
 * @param b pointer of b
 * @param c pointer of c
 *
 * @return
 *     - ESP_OK Success
 *     - ESP_FAIL Fail
 */
esp_err_t apds9960_get_color_data(apds9960_handle_t sensor, uint16_t *r,
                                      uint16_t *g, uint16_t *b, uint16_t *c);

/**
 * @brief  Converts the raw R/G/B values to color temperature in degrees Kelvin
 *
 * @param sensor object handle of apds9960
 * @param r value of r
 * @param g value of g
 * @param b value of b
 *
 * @return
 *     - Return the results in degrees Kelvin
 */
uint16_t apds9960_calculate_color_temperature(apds9960_handle_t sensor,
        uint16_t r, uint16_t g, uint16_t b);

/**
 * @brief  Calculate ambient light values
 *
 * @param sensor object handle of apds9960
 * @param r value of r
 * @param g value of g
 * @param b value of b
 *
 * @return
 *     - Return the results of ambient light values
 */
uint16_t apds9960_calculate_lux(apds9960_handle_t sensor, uint16_t r,
                                    uint16_t g, uint16_t b);

/**
 * @brief Turns color interrupts on or off
 *
 * @param sensor object handle of apds9960
 * @param en true to enable interrupts, false to disable interrupts
 *
 * @return
 *     - ESP_OK Success
 *     - ESP_FAIL Fail
 */
esp_err_t apds9960_enable_color_interrupt(apds9960_handle_t sensor, bool en);


/**
 * @brief Set ALS interrupt low/high threshold
 *
 * @param sensor object handle of apds9960
 * @param l low threshold
 * @param h high threshold
 *
 * @return
 *     - ESP_OK Success
 *     - ESP_FAIL Fail
 */
esp_err_t apds9960_set_int_limits(apds9960_handle_t sensor, uint16_t l,
                                      uint16_t h);

/**
 * @brief clear interrupt
 *
 * @param sensor object handle of apds9960
 *
 * @return
 *     - ESP_OK Success
 *     - ESP_FAIL Fail
 */
esp_err_t apds9960_clear_interrupt(apds9960_handle_t sensor);

/**
 * @brief enable apds9960 device
 *
 * @param sensor object handle of apds9960
 * @param en true to enable sensor, false to disable sensor
 *
 * @return
 *     - ESP_OK Success
 *     - ESP_FAIL Fail
 */
esp_err_t apds9960_enable(apds9960_handle_t sensor, bool en);

/**
 * @brief Sets the low threshold for ambient light interrupts
 *
 * @param sensor object handle of apds9960
 * @param threshold low threshold value for interrupt to trigger
 *
 * @return
 *     - ESP_OK Success
 *     - ESP_FAIL Fail
 */
esp_err_t apds9960_set_light_intlow_threshold(apds9960_handle_t sensor,
        uint16_t threshold);

/**
 * @brief Sets the high threshold for ambient light interrupts
 *
 * @param sensor object handle of apds9960
 * @param threshold high threshold value for interrupt to trigger
 *
 * @return
 *     - ESP_OK Success
 *     - ESP_FAIL Fail
 */
esp_err_t apds9960_set_light_inthigh_threshold(apds9960_handle_t sensor,
        uint16_t threshold);

#ifdef __cplusplus
}
#endif

#endif
