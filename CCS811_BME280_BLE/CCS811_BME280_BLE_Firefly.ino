/* 01/14/2021 Copyright Tlera Corporation
 *  
 *  Created by Kris Winer
 *  
 *  The AMS CCS811 is an air quality sensor that provides equivalent CO2 and volatile organic measurements from direct
 *  I2C register reads as well as current and voltage (effective resistance of the sensing element). Gas sensors, including 
 *  this MEMs gas sensor in the CCS811 measure resistance of a substrate that changes when exposed to inert gasses and 
 *  volatile organic compounds. Changed in concentration vary exponentially with the changes in resistance. The CCS811
 *  has an embedded ASIC calibrated against most common indoor pollutants that returns a good estimate of
 *  equivalent CO2 concentration in parts per million (400 - 8192 range) and volatile organic compounds in parts per billion (0 - 1187).
 *  The sensor is quite sensitive to breath and other human emissions.
 *  
 *  This sketch uses default SDA/SCL pins on the Firefly development board.
 *  The BME280 is a simple but high resolution pressure/humidity/temperature sensor, which can be used in its high resolution
 *  mode but with power consumption of 20 microAmp, or in a lower resolution mode with power consumption of
 *  only 1 microAmp. The choice will depend on the application.
 
 Library may be used freely and without limit with attribution.
 
  */
#include <Arduino.h>
#include "STM32WB.h"
#include "BLE.h"
#include <RTC.h>
#include "BME280.h"
#include "CCS811.h"
#include "I2Cdev.h"

#define I2C_BUS          Wire               // Define the I2C bus (Wire instance) you wish to use

I2Cdev                   i2c_0(&I2C_BUS);   // Instantiate the I2Cdev object and point to the desired I2C bus

#define SerialDebug true  // set to true to get Serial output for debugging
#define myLed LED_BUILTIN // blue led

uint8_t oldbatteryValue = 0, batteryValue = 0;
int16_t temperatureValue = 0, oldtemperatureValue = 0;
uint16_t humidityValue = 0, oldhumidityValue = 0;
uint32_t pressureValue = 0, oldpressureValue = 0;

//Environmental Sensing Service
BLEService env_sensingService("181A");
BLECharacteristic pressureCharacteristic("2A6D", (BLE_PROPERTY_READ | BLE_PROPERTY_NOTIFY), sizeof(pressureValue) ); // pressure is uint32_t
BLEDescriptor pressureDescriptor("2901", "Pressure Pa x 10");
BLECharacteristic temperatureCharacteristic("2A6E", (BLE_PROPERTY_READ | BLE_PROPERTY_NOTIFY), sizeof(temperatureValue) ); // temperature is int16_t
BLEDescriptor temperatureDescriptor("2901", "Temperature C x 100");
BLECharacteristic humidityCharacteristic("2A6F", (BLE_PROPERTY_READ | BLE_PROPERTY_NOTIFY), sizeof(humidityValue) ); // humidity is uint16_t
BLEDescriptor humidityDescriptor("2901", "Humidity %rH x 100");

// Battery Service
BLEService batteryService("180F");
BLECharacteristic battlevelCharacteristic("2A19", (BLE_PROPERTY_READ | BLE_PROPERTY_NOTIFY), sizeof(batteryValue)); // battery level is uint8_t
BLEDescriptor battlevelDescriptor("2901", "Battery Level 0 - 100");

// BME280 definitions
/* Specify BME280 configuration
 *  Choices are:
 P_OSR_01, P_OSR_02, P_OSR_04, P_OSR_08, P_OSR_16 // pressure oversampling
 H_OSR_01, H_OSR_02, H_OSR_04, H_OSR_08, H_OSR_16 // humidity oversampling
 T_OSR_01, T_OSR_02, T_OSR_04, T_OSR_08, T_OSR_16 // temperature oversampling
 full, BW0_223ODR,BW0_092ODR, BW0_042ODR, BW0_021ODR // bandwidth at 0.021 x sample rate
 BME280Sleep, forced,, forced2, normal //operation modes
 t_00_5ms = 0, t_62_5ms, t_125ms, t_250ms, t_500ms, t_1000ms, t_10ms, t_20ms // determines sample rate
 */
uint8_t Posr = P_OSR_01, Hosr = H_OSR_01, Tosr = T_OSR_01, Mode = BME280Sleep, IIRFilter = full, SBy = t_1000ms;     // set pressure amd temperature output data rate

float Temperature, Pressure, Humidity;              // stores BME280 pressures sensor pressure and temperature
int32_t rawPress, rawTemp, rawHumidity, compTemp;   // pressure and temperature raw count output for BME280
uint32_t compHumidity, compPress;                   // variables to hold raw BME280 humidity value

float temperature_C, temperature_F, pressure, humidity, altitude; // Scaled output of the BME280

BME280 BME280(&i2c_0); // instantiate BME280 class


// RTC set up
/* Change these values to set the current initial time */
uint8_t seconds, minutes, hours, day, month, year;
uint8_t Seconds, Minutes, Hours, Day, Month, Year;
volatile bool alarmFlag = false; // for RTC alarm interrupt

float VDDA, VBAT, STTemperature, oldSTTemperature;


// CCS811 definitions
#define CCS811_intPin  8
#define CCS811_wakePin 9

/* Specify CCS811 sensor parameters
 *  Choices are   dt_idle , dt_1sec, dt_10sec, dt_60sec
 */
uint8_t AQRate = dt_10sec;  // set the sample rate
uint8_t rawData[8] = {0, 0, 0, 0, 0, 0, 0, 0};  // array to hold the raw data
uint16_t eCO2 = 0, TVOC = 0;
uint8_t Current = 0;
float Voltage = 0.0f;

volatile bool newCCS811Data  = false; // boolean flag for interrupt

CCS811 CCS811(&i2c_0); // instantiate CCS811 class

void setup()
{
  pinMode(CCS811_wakePin, OUTPUT);
  //Enable the CCS811 for I2C scan
  digitalWrite(CCS811_wakePin, LOW); // set LOW to enable the CCS811 air quality sensor

  Serial.begin(115200);
  delay(4000);
  Serial.println("Serial enabled!");

  pinMode(myLed, OUTPUT);
  digitalWrite(myLed, HIGH); // start with led on

  pinMode(CCS811_intPin, INPUT); // active LOW

  I2C_BUS.begin();                   // Set master mode, default on SDA/SCL for STM32L4
  I2C_BUS.setClock(400000);          // I2C frequency at 400 kHz
  delay(1000);

  pinMode(CCS811_wakePin, OUTPUT);
  //Enable the CCS811 for I2C scan
  digitalWrite(CCS811_wakePin, LOW); // set LOW to enable the CCS811 air quality sensor
  
  Serial.println("Scan for I2C devices:");
  i2c_0.I2Cscan();                   // should detect BME280 at 0x76 and CCS811 at 0x5A
  delay(1000);

  //Disable the CCS811 for I2C scan
  digitalWrite(CCS811_wakePin, HIGH); // set LOW to enable the CCS811 air quality sensor
  delay(1000);

  // Read the WHO_AM_I register of the BME280 this is a good test of communication
  byte d = BME280.getChipID();  // Read WHO_AM_I register for BME280
  Serial.print("BME280 "); Serial.print("I AM "); Serial.print(d, HEX); Serial.print(" I should be "); Serial.println(0x60, HEX);
  Serial.println(" ");
  delay(1000); 

  // Read the WHO_AM_I register of the CCS811 this is a good test of communication
  digitalWrite(CCS811_wakePin, LOW); // set LOW to enable the CCS811 air quality sensor
  byte e = CCS811.getChipID();
  digitalWrite(CCS811_wakePin, HIGH); // set HIGH to disable the CCS811 air quality sensor
  Serial.print("CCS811 "); Serial.print("I AM "); Serial.print(e, HEX); Serial.print(" I should be "); Serial.println(0x81, HEX);
  Serial.println(" ");
  delay(1000); 
  
  if(d == 0x60 && e == 0x81 ) {

   Serial.println("BME280+CCS811 are online..."); Serial.println(" ");
   digitalWrite(myLed, LOW);

   BME280.resetBME280();                                                        // reset BME280 before initilization
   delay(100);
   BME280.BME280Init(Posr, Hosr, Tosr, Mode, IIRFilter, SBy);                   // Initialize BME280 altimeter
   BME280.BME280forced();                                                       // get initial data sample, then go back to sleep

   // initialize CCS811 and check version and status
   digitalWrite(CCS811_wakePin, LOW); // set LOW to enable the CCS811 air quality sensor
   CCS811.CCS811init(AQRate);
   digitalWrite(CCS811_wakePin, HIGH); // set HIGH to disable the CCS811 air quality sensor

  }
  else 
  {
  if(d != 0x60) Serial.println(" BME280 not functioning!");    
  if(e != 0x81) Serial.println(" CCS811 not functioning!"); 
  }
  
  BLE.begin();
  BLE.setLocalName("Firefly");

  // Instantiate Environmental Service  
  BLE.setAdvertisedServiceUuid(env_sensingService.uuid());
  env_sensingService.addCharacteristic(temperatureCharacteristic);
  env_sensingService.addCharacteristic(pressureCharacteristic);
  env_sensingService.addCharacteristic(humidityCharacteristic);
  BLE.addService(env_sensingService);  
 
  BME280.BME280forced();  // get one data sample, then go back to sleep

  rawTemp =  BME280.readBME280Temperature();
  compTemp = BME280.BME280_compensate_T(rawTemp);
  oldtemperatureValue = (int16_t) (compTemp);
  temperatureCharacteristic.setValue((uint8_t *) &oldtemperatureValue, sizeof(oldtemperatureValue)); // temperature in C x 100
     
  rawPress =  BME280.readBME280Pressure();
  pressure = (float) BME280.BME280_compensate_P(rawPress)/25600.f; // Pressure in mbar
  oldpressureValue = (uint32_t) (1000.0f * pressure);  // convert to Pa x 10
  pressureCharacteristic.setValue((uint8_t *) &oldpressureValue, sizeof(oldpressureValue)); // Pa x 10
   
  rawHumidity =  BME280.readBME280Humidity();
  compHumidity = BME280.BME280_compensate_H(rawHumidity);
  humidity = (float)compHumidity/1024.0f; // Humidity in %RHoldST
  oldhumidityValue = (uint16_t) (100* humidity);
  humidityCharacteristic.setValue((uint8_t *) &oldhumidityValue, sizeof(oldhumidityValue)); // %rH x 100
  
  // Instantiate Battery Service  
  BLE.setAdvertisedServiceUuid(batteryService.uuid());
  batteryService.addCharacteristic(battlevelCharacteristic);
  BLE.addService(batteryService);  
 
  VBAT = STM32WB.readBattery();
  oldbatteryValue = (uint8_t) ( 100.0f * (VBAT - 3.5f)/(4.2f - 3.5f) );
  battlevelCharacteristic.setValue(&batteryValue, sizeof(oldbatteryValue));

  /* Set up the RTC alarm interrupt */
//  RTC.setAlarmTime(16, 0, 10);    // Arbitrary alarm time 
  RTC.enableAlarm(RTC_MATCH_ANY); // Alarm once a second
  
  RTC.attachInterrupt(alarmMatch); // interrupt every time the alarm sounds
  attachInterrupt(CCS811_intPin,  myinthandler, FALLING); // enable CCS811 interrupt 

  BLE.advertise();
}

void loop()
{
    if( !BLE.advertising() && !BLE.connected()) { // check if still connected, if not advertise again
     BLE.advertise();
    }
 
    // CCS811 data
    // If intPin goes LOW, all data registers have new data
    if(newCCS811Data == true) {  // On interrupt, read data
    newCCS811Data = false;  // reset newData flag
     
    digitalWrite(CCS811_wakePin, LOW); // set LOW to enable the CCS811 air quality sensor
    CCS811.readCCS811Data(rawData);
    CCS811.compensateCCS811(compHumidity, compTemp); // compensate CCS811 using BME280 humidity and temperature
    digitalWrite(CCS811_wakePin, HIGH); // set LOW to enable the CCS811 air quality sensor 

    eCO2 = (uint16_t) ((uint16_t) rawData[0] << 8 | rawData[1]);
    TVOC = (uint16_t) ((uint16_t) rawData[2] << 8 | rawData[3]);
    Current = (rawData[6] & 0xFC) >> 2;
    Voltage = (float) ((uint16_t) ((((uint16_t)rawData[6] & 0x02) << 8) | rawData[7])) * (1.65f/1023.0f); 
    
    Serial.println("CCS811:");
    Serial.print("Eq CO2 in ppm = "); Serial.println(eCO2);
    Serial.print("TVOC in ppb = "); Serial.println(TVOC);
    Serial.print("Sensor current (uA) = "); Serial.println(Current);
    Serial.print("Sensor voltage (V) = "); Serial.println(Voltage, 2);  
    Serial.println(" ");
  }
            
   /*RTC Timer*/
   if (alarmFlag) { // update serial output whenever there is a timer alarm
      alarmFlag = false;

    /* BME280 sensor data */
    BME280.BME280forced();  // get one data sample, then go back to sleep

    rawTemp =  BME280.readBME280Temperature();
    compTemp = BME280.BME280_compensate_T(rawTemp);
    temperature_C = (float) compTemp/100.0f;
    temperature_F = 9.0f*temperature_C/5.0f + 32.0f;
     
    rawPress =  BME280.readBME280Pressure();
    pressure = (float) BME280.BME280_compensate_P(rawPress)/25600.f; // Pressure in mbar
    altitude = 145366.45f*(1.0f - powf((pressure/1013.25f), 0.190284f));   
   
    rawHumidity =  BME280.readBME280Humidity();
    compHumidity = BME280.BME280_compensate_H(rawHumidity);
    humidity = (float)compHumidity/1024.0f; // Humidity in %RH
 
    Serial.println("BME280:");
    Serial.print("Altimeter temperature = "); 
    Serial.print( temperature_C, 2); 
    Serial.println(" C"); // temperature in degrees Celsius
    Serial.print("Altimeter temperature = "); 
    Serial.print(temperature_F, 2); 
    Serial.println(" F"); // temperature in degrees Fahrenheit
    Serial.print("Altimeter pressure = "); 
    Serial.print(pressure, 2);  
    Serial.println(" mbar");// pressure in millibar
    Serial.print("Altitude = "); 
    Serial.print(altitude, 2); 
    Serial.println(" feet");
    Serial.print("Altimeter humidity = "); 
    Serial.print(humidity, 1);  
    Serial.println(" %RH");// pressure in millibar
    Serial.println(" ");

    // Read RTC
    Serial.println("RTC:");
    Day = RTC.getDay();
    Month = RTC.getMonth();
    Year = RTC.getYear();
    Seconds = RTC.getSeconds();
    Minutes = RTC.getMinutes();
    Hours   = RTC.getHours();     
    if(Hours < 10) {Serial.print("0"); Serial.print(Hours);} else Serial.print(Hours);
    Serial.print(":"); 
    if(Minutes < 10) {Serial.print("0"); Serial.print(Minutes);} else Serial.print(Minutes); 
    Serial.print(":"); 
    if(Seconds < 10) {Serial.print("0"); Serial.println(Seconds);} else Serial.println(Seconds);  

    Serial.print(Month); Serial.print("/"); Serial.print(Day); Serial.print("/"); Serial.println(Year);
    Serial.println(" ");

    VBAT = STM32WB.readBattery();
    Serial.print("VBAT = "); Serial.println(VBAT, 2); 

    temperatureValue = (int16_t) (compTemp);
    if( !isnan(temperatureValue) && significantChange(oldtemperatureValue, temperatureValue, 1) ) { // only write value when value changes by 0.01 C
    // sends the data LSByte first, resolved to 0.01 C
    // sending temperature as Centigrade x 100
    temperatureCharacteristic.setValue((uint8_t *)&temperatureValue, sizeof(temperatureValue));  
    oldtemperatureValue = temperatureValue;
    }
     
    pressureValue = (uint32_t) (1000.0f * pressure);  // convert to Pa x 10
    if( !isnan(pressureValue) && significantChange(oldpressureValue, pressureValue, 1) ) { // only write value when value changes by 10 Pa
    // sends the data LSByte first, resolved to 0.1 Pa
    // sending data as Pa x 10
    pressureCharacteristic.setValue((uint8_t *) &pressureValue, sizeof(pressureValue)); // Pa x 10
    oldpressureValue = pressureValue;
    }
   
    humidityValue = (uint16_t) (100.0f * humidity);
    if( !isnan(humidityValue) && significantChange(oldhumidityValue, humidityValue, 10) ) { // only write value when value changes by 0.1%
    // sends the data LSByte first, resolved to 0.01 %rH
    // sending data as % rH x 100
    humidityCharacteristic.setValue((uint8_t *) &humidityValue, sizeof(humidityValue)); // %rH x 100
    oldhumidityValue = humidityValue;
    }

    if (VBAT < 4.2f && VBAT > 3.5f) {
      batteryValue = (uint8_t) ( 100.0f * (VBAT - 3.5f)/(4.2f - 3.5f) ); // uint8_t representing percent charge
    }
    else
    {
      batteryValue = 0;
    }

    if( !isnan(batteryValue) && significantChange(oldbatteryValue, batteryValue, 1) ) { // only write value when value changes by 1 percent or more
    battlevelCharacteristic.writeValue(&batteryValue, sizeof(batteryValue));
    oldbatteryValue = batteryValue;
    }
      
    digitalWrite(myLed, HIGH); delay(1); digitalWrite(myLed, LOW); // blink led at end of loop
    }  
         
      STM32WB.stop();    // time out in stop mode to save power
}

//===================================================================================================================
//====== Set of useful functions
//===================================================================================================================

void myinthandler()
{
  newCCS811Data = true;
  STM32WB.wakeup();
}


void alarmMatch()
{
  alarmFlag = true;
  STM32WB.wakeup();
}


boolean significantChange(float val1, float val2, float threshold) {
  return (abs(val1 - val2) >= threshold);
}
