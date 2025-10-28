#include "MQUnifiedsensor.h"
#include "espidf_adc_helper.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <float.h>
#include <math.h>

#define retries 2
#define retry_interval 20

MQUnifiedsensor::MQUnifiedsensor(float Voltage_Resolution, int ADC_Bit_Resolution, int pin, const char* type) {
  this->_pin = pin;
  strncpy(this->_type, type, sizeof(this->_type)-1);
  this->_type[sizeof(this->_type)-1] = '\0';
  this->_VOLT_RESOLUTION = Voltage_Resolution;
  this->_VCC = Voltage_Resolution;
  this->_ADC_Bit_Resolution = ADC_Bit_Resolution;
}
static inline double safePow(double base, double exp){
  if(exp == 0.0) return 1.0;
  if(exp == 1.0) return base;
  if(exp == 2.0) return base * base;
  return pow(base, exp);
}
static inline bool willOverflow(double log_ppm){
  static const double maxLog = log10((double)FLT_MAX);
  static const double minLog = log10((double)FLT_MIN);
  return (log_ppm > maxLog || log_ppm < minLog);
}

void MQUnifiedsensor::setA(float a) {
  if(isinf(a) || isnan(a)) {
    this->_a = 0;
  } else if(a > MQ_MAX_A) {
    this->_a = MQ_MAX_A;
  } else if(a < -MQ_MAX_A) {
    this->_a = -MQ_MAX_A;
  } else {
    this->_a = a;
  }
}
void MQUnifiedsensor::setB(float b) {
  if(isinf(b) || isnan(b)) {
    this->_b = 0;
  } else if(b > MQ_MAX_B) {
    this->_b = MQ_MAX_B;
  } else if(b < -MQ_MAX_B) {
    this->_b = -MQ_MAX_B;
  } else {
    this->_b = b;
  }
}
void MQUnifiedsensor::setR0(float R0) {
  this->_R0 = R0;
}
void MQUnifiedsensor::setRL(float RL) {
  this->_RL = RL;
}
void MQUnifiedsensor::setADC(int value)
{
  this-> _sensor_volt = (value) * _VOLT_RESOLUTION / ((pow(2, _ADC_Bit_Resolution)) - 1); 
  this-> _adc =  value;
}
void MQUnifiedsensor::setVoltResolution(float voltage_resolution)
{
  _VOLT_RESOLUTION = voltage_resolution;
}
void MQUnifiedsensor::setVCC(float vcc)
{
  _VCC = vcc;
}
void MQUnifiedsensor::setPin(int pin) {
  this->_pin = pin;
}
void MQUnifiedsensor::setRegressionMethod(int regressionMethod)
{
  //this->_regressionMethod = regressionMethod;
  this->_regressionMethod = regressionMethod;
}
float MQUnifiedsensor::getR0() {
  return _R0;
}
float MQUnifiedsensor::getRL() {
  return _RL;
}
float MQUnifiedsensor::getVoltResolution()
{
  return _VOLT_RESOLUTION;
}
float MQUnifiedsensor::getVCC()
{
  return _VCC;
}
const char* MQUnifiedsensor::getRegressionMethod()
{
  return (_regressionMethod == 1) ? "Exponential" : "Linear";
}
float MQUnifiedsensor::getA() {
  return _a;
}
float MQUnifiedsensor::getB() {
  return _b;
}
void MQUnifiedsensor::serialDebug(bool onSetup)
{
  if(onSetup)
  {
    ESP_LOGI("MQUnifiedsensor", "************************************************************************************************************************************************");
    ESP_LOGI("MQUnifiedsensor", "MQ sensor reading library for ESP32");
    ESP_LOGI("MQUnifiedsensor", "Note: remember that all the parameters below can be modified during the program execution with the methods:");
    ESP_LOGI("MQUnifiedsensor", "setR0, setRL, setA, setB where you will have to send as parameter the new value, example: mySensor.setR0(20); //R0 = 20KΩ");
    ESP_LOGI("MQUnifiedsensor", "Authors: Miguel A. Califa U - Yersson R. Carrillo A - Ghiordy F. Contreras C");
    ESP_LOGI("MQUnifiedsensor", "Contributors: Andres A. Martinez - Juan A. Rodríguez - Mario A. Rodríguez O ");
    ESP_LOGI("MQUnifiedsensor", "Contributor for ESP32: Carlos Delfino");
    ESP_LOGI("MQUnifiedsensor", "Sensor: %s", _type);
    ESP_LOGI("MQUnifiedsensor", "ADC voltage: %.2f VDC", _VOLT_RESOLUTION);
    ESP_LOGI("MQUnifiedsensor", "Sensor supply (VCC): %.2f VDC", _VCC);
    ESP_LOGI("MQUnifiedsensor", "ADC Resolution: %d Bits", _ADC_Bit_Resolution);
    ESP_LOGI("MQUnifiedsensor", "R0: %.2f KΩ", _R0);
    ESP_LOGI("MQUnifiedsensor", "RL: %.2f KΩ", _RL);
    ESP_LOGI("MQUnifiedsensor", "Model: %s", _regressionMethod == 1 ? "Exponential" : "Linear");
    ESP_LOGI("MQUnifiedsensor", "%s -> a: %.2f | b: %.2f", _type, _a, _b);
    ESP_LOGI("MQUnifiedsensor", "Development board: %s", _placa);
  }
  else 
  {
    if(!_firstFlag)
    {
      ESP_LOGI("MQUnifiedsensor", "| ******************************************************************** %s *********************************************************************|", _type);
      ESP_LOGI("MQUnifiedsensor", "|ADC_In | Equation_V_ADC | Voltage_ADC |        Equation_RS        | Resistance_RS  |    EQ_Ratio  | Ratio (RS/R0) | Equation_PPM |     PPM    |");
      _firstFlag = true;  //Headers are printed
    }
    else
    {
      ESP_LOGI("MQUnifiedsensor", "| %d | v = ADC*%.2f/%d  | %.2f | RS = ((%.2f*%.2f)/%.2f) - %.2f | %.2f | %.2f | %.2f | %s | %.2f |",
        _adc, _VOLT_RESOLUTION, (pow(2, _ADC_Bit_Resolution)) - 1, _sensor_volt,
        _VCC, _RL, _sensor_volt, _RL, _RS_Calc, _ratio,
        _regressionMethod == 1 ? "ratio*a + b" : "pow(10, (log10(ratio)-b)/a)", _PPM);
    }
  }
}
void MQUnifiedsensor::update()
{
  _sensor_volt = this->getVoltage();
}
void MQUnifiedsensor::externalADCUpdate(float volt)
{
  _sensor_volt = volt;
}
float MQUnifiedsensor::validateEcuation(float ratioInput)
{
  double ppm;
  if(_regressionMethod == 1){
      if(ratioInput <= 0 || _a == 0) return 0;
      double logppm = log10((double)_a) + (double)_b * log10((double)ratioInput);
      if(willOverflow(logppm)) ppm = (logppm > 0) ? FLT_MAX : 0.0;
      else ppm = safePow(10.0, logppm);
  }
  else
  {
      if(ratioInput <= 0 || _a == 0) return 0;
      double logppm = (log10((double)ratioInput)-(double)_b)/(double)_a;
      if(willOverflow(logppm)) ppm = (logppm > 0) ? FLT_MAX : 0.0;
      else ppm = safePow(10.0, logppm);
  }
  if(isinf(ppm) || isnan(ppm)) ppm = FLT_MAX;
  _PPM = (float)ppm;
  return _PPM;
}
float MQUnifiedsensor::readSensor(bool isMQ303A, float correctionFactor, bool injected)
{
  //More explained in: https://jayconsystems.com/blog/understanding-a-gas-sensor

  float voltRes = _VOLT_RESOLUTION; // preserve global resolution
  if(isMQ303A) {
    voltRes = voltRes - 0.45; //Calculations for RS using mq303a sensor look wrong #42
  }
  _RS_Calc = ((_VCC*_RL)/_sensor_volt)-_RL; //Get value of RS in a gas
  if(_RS_Calc < 0)  _RS_Calc = 0; //No negative values accepted.
  if(!injected) _ratio = _RS_Calc / this->_R0;   // Get ratio RS_gas/RS_air
  _ratio += correctionFactor;
  if(_ratio <= 0)  _ratio = 0; //No negative values accepted or upper datasheet recommendation.
  double ppm;
  if(_regressionMethod == 1){
    if(_ratio <= 0 || _a == 0) return 0;
    double logppm = log10((double)_a) + (double)_b * log10((double)_ratio);
    if(willOverflow(logppm)) ppm = (logppm > 0) ? FLT_MAX : 0.0;
    else ppm = safePow(10.0, logppm);
  }
  else
  {
    if(_ratio <= 0 || _a == 0) return 0;
    double logppm = (log10((double)_ratio)-(double)_b)/(double)_a;
    if(willOverflow(logppm)) ppm = (logppm > 0) ? FLT_MAX : 0.0;
    else ppm = safePow(10.0, logppm);
  }
  if(ppm < 0)  ppm = 0; //No negative values accepted or upper datasheet recommendation.
  if(isinf(ppm) || isnan(ppm)) ppm = FLT_MAX;
  _PPM = (float)ppm;
  //if(_PPM > 10000) _PPM = 99999999; //No negative values accepted or upper datasheet recommendation.
  return _PPM;
}
float MQUnifiedsensor::readSensorR0Rs(float correctionFactor)
{
  //More explained in: https://jayconsystems.com/blog/understanding-a-gas-sensor
  _RS_Calc = ((_VCC*_RL)/_sensor_volt)-_RL; //Get value of RS in a gas
  if(_RS_Calc < 0)  _RS_Calc = 0; //No negative values accepted.
  _ratio = this->_R0/_RS_Calc;   // Get ratio RS_air/RS_gas <- INVERTED for MQ-131 issue 28 https://github.com/miguel5612/MQSensorsLib/issues/28
  _ratio += correctionFactor;
  if(_ratio <= 0)  _ratio = 0; //No negative values accepted or upper datasheet recommendation.
  double ppm;
  if(_regressionMethod == 1){
    if(_ratio <= 0 || _a == 0) return 0;
    double logppm = log10((double)_a) + (double)_b * log10((double)_ratio);
    if(willOverflow(logppm)) ppm = (logppm > 0) ? FLT_MAX : 0.0;
    else ppm = safePow(10.0, logppm);
  }
  else
  {
    if(_ratio <= 0 || _a == 0) return 0;
    double logppm = (log10((double)_ratio)-(double)_b)/(double)_a;
    if(willOverflow(logppm)) ppm = (logppm > 0) ? FLT_MAX : 0.0;
    else ppm = safePow(10.0, logppm);
  }
  if(ppm < 0)  ppm = 0; //No negative values accepted or upper datasheet recommendation.
  if(isinf(ppm) || isnan(ppm)) ppm = FLT_MAX;
  _PPM = (float)ppm;
  //if(_PPM > 10000) _PPM = 99999999; //No negative values accepted or upper datasheet recommendation.
  return _PPM;
}
float MQUnifiedsensor::calibrate(float ratioInCleanAir, float correctionFactor) {
  //More explained in: https://jayconsystems.com/blog/understanding-a-gas-sensor
  /*
  V = I x R 
  VRL = [VC / (RS + RL)] x RL 
  VRL = (VC x RL) / (RS + RL) 
  Así que ahora resolvemos para RS: 
  VRL x (RS + RL) = VC x RL
  (VRL x RS) + (VRL x RL) = VC x RL 
  (VRL x RS) = (VC x RL) - (VRL x RL)
  RS = [(VC x RL) - (VRL x RL)] / VRL
  RS = [(VC x RL) / VRL] - RL
  */
  float RS_air; //Define variable for sensor resistance
  float R0; //Define variable for R0
  RS_air = ((_VCC*_RL)/_sensor_volt)-_RL; //Calculate RS in fresh air
  if(RS_air < 0)  RS_air = 0; //No negative values accepted.
  R0 = RS_air/ratioInCleanAir; //Calculate R0
  R0 += correctionFactor;
  if(R0 < 0)  R0 = 0; //No negative values accepted.
  return R0;
}
float MQUnifiedsensor::getVoltage(bool read, bool injected, int value) {
  float voltage;
  if(read)
  {
    float avg = 0.0;
    for (int i = 0; i < retries; i ++) {
      _adc = espidf_analogRead(this->_pin, _ADC_Bit_Resolution);
      avg += _adc;
      vTaskDelay(retry_interval / portTICK_PERIOD_MS);
    }
    voltage = (avg/ retries) * _VOLT_RESOLUTION / ((pow(2, _ADC_Bit_Resolution)) - 1);
  }
  else if(!injected)
  {
    voltage = _sensor_volt;
  }
  else 
  {
    voltage = (value) * _VOLT_RESOLUTION / ((pow(2, _ADC_Bit_Resolution)) - 1);
    _sensor_volt = voltage; //to work on testing
  }
  return voltage;
}
float MQUnifiedsensor:: setRsR0RatioGetPPM(float value)
{
  _ratio = value;
  return readSensor(false, 0, true);
}
float MQUnifiedsensor::getRS()
{
  //More explained in: https://jayconsystems.com/blog/understanding-a-gas-sensor
  _RS_Calc = ((_VCC*_RL)/_sensor_volt)-_RL; //Get value of RS in a gas
  if(_RS_Calc < 0)  _RS_Calc = 0; //No negative values accepted.
  return _RS_Calc;
}

float MQUnifiedsensor::stringTofloat(const char* str)
{
  return str ? atof(str) : 0.0f;
}
