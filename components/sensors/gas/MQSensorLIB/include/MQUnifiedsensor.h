#ifndef MQUnifiedsensor_H
#define MQUnifiedsensor_H

#include <stdint.h>
#include <float.h>
#include <math.h>

// Maximum coefficients allowed for the regression model
#define MQ_MAX_A 1e30
#define MQ_MAX_B 100.0

/***********************Software Related Macros************************************/

class MQUnifiedsensor
{
  public:
    MQUnifiedsensor(float Voltage_Resolution =  5, int ADC_Bit_Resolution = 10, int pin = 1, const char* type = "CUSTOM MQ");
    
    //Functions to set values
    void init();
    void update();
    void externalADCUpdate(float volt);
    void setR0(float R0 = 10);
    void setRL(float RL = 10);
    void setA(float a);
    void setB(float b);
    void setRegressionMethod(int regressionMethod);
    void setVoltResolution(float voltage_resolution =  5);
    void setVCC(float vcc = 5);
    void setPin(int pin = 1);
    void serialDebug(bool onSetup = false); //Show on serial port information about sensor
    void setADC(int value); //For external ADC Usage
    void setType(const char* type);
    void setBoard(const char* placa);

    //user functions
    float calibrate(float ratioInCleanAir, float correctionFactor = 0.0);
    float readSensor(bool isMQ303A = false, float correctionFactor = 0.0, bool injected=false);
    float readSensorR0Rs(float correctionFactor = 0.0);
    float validateEcuation(float ratioInput = 0);
    
    //get function for info
    float getA();
    float getB();
    float getR0();
    float getRL();
    float getVoltResolution();
    float getVCC();
    const char* getRegressionMethod();
    float getVoltage(bool read = true, bool injected = false, int value = 0);
    float stringTofloat(const char* str);

    // functions for testing
    float getRS();    
    float setRsR0RatioGetPPM(float value);

  private:
    /************************Private vars************************************/
    uint8_t _pin = 1;
    bool _firstFlag = false;
    float _VOLT_RESOLUTION  = 5.0; // if 3.3v use 3.3
    float _VCC = 5.0; // Sensor supply voltage
    float _RL = 10; //Value in KiloOhms
    uint8_t _ADC_Bit_Resolution = 10;
    uint8_t _regressionMethod = 1; // 1 -> Exponential || 2 -> Linear
    
    float _adc, _a, _b, _sensor_volt;
    float  _R0, RS_air, _ratio, _PPM, _RS_Calc;  
    
    char _type[16];
    char _placa[20]; 
};

#endif //MQUnifiedsensor_H
