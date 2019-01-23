#include <Arduino.h>
#include <Wire.h>

#include "SerialPnP.h"
#include "OneWire.h"

// Control logic
// Optimal pH is 5.8, so we don't want to make adjustments until it is
// either less than 5.5 or greater than 6.1
#define PH_ADJUST_THRESHOLD_MIN 5.5f
#define PH_ADJUST_THRESHOLD_MAX 6.1f
// Logic is only active while we are in the 5.2 <-> 6.4 range
// to prevent overdosing
#define PH_ADJUST_LOGIC_ACTIVE_MIN 5.2f
#define PH_ADJUST_LOGIC_ACTIVE_MAX 6.4f
// Dosing parameters
#define PH_ADJUST_DOSE_TIME       2000 // 8s
#define PH_ADJUST_DOSE_INTERVAL   (5*60*1000UL) // 5m

#define PH_ADJUST_DOSE_PUMPWARMUP 2000

// Dose variables
uint32_t g_millisFinishDose = 0;
uint32_t g_millisLastDose = 0;

// Pin definitions for accessories
#define PDO_RELAY_LIGHT   2
#define PDO_RELAY_WATER   3
#define PAI_FLOW          A0
#define PDO_MOTOR12_EN    6
#define PDO_MOTOR34_EN    7
#define PDI_TEMP_WATER    8 // Onewire interface
#define PAI_TEMP_AIR      A3
#define PAI_LIGHT         A2
#define PAI_DEPTH_WATER   A1
#define PDO_LED1          9
#define PDO_LED2          5

#define PDO_MOTOR1        13
#define PDO_MOTOR2        12
#define PDO_MOTOR3        11
#define PDO_MOTOR4        10

#define I2C_EC            0x64
#define I2C_PH            0x65

#define I2C_EC_REG_POWER  0x06
#define I2C_EC_REG_STATUS 0x07
#define I2C_EC_REG_DATA   0x18

#define I2C_EC_SAMPLE_DELAY 50

#define I2C_PH_REG_POWER  0x06
#define I2C_PH_REG_STATUS 0x07
#define I2C_PH_REG_DATA   0x16
#define I2C_PH_REG_CALIB  0x08
#define I2C_PH_REG_CALIB_START \
                          0x0C
#define I2C_PH_TEMP       0x0E

#define I2C_PH_CALIB_DELAY  1500
//#define I2C_PH_POWER_DELAY  500
#define I2C_PH_SAMPLE_DELAY 50

#define WATER_TEMP_SENSOR
//#define DOSE_PH

#ifdef WATER_TEMP_SENSOR
OneWire g_OneWireWaterTemp(PDI_TEMP_WATER);
#endif

int32_t g_sampleRate = 5000;
long g_lastSample = 0;
byte g_tempSensorAddress[8];

void CbSampleRate(int32_t *input, int32_t *output) {
  if (output) *output = g_sampleRate;
  if (input) g_sampleRate = *input;
}

void CbCalibratePh(float *finput, int32_t *output) {
    if (output) *output = 1;

    uint32_t input = (*finput * 1000);

    // Clear calibration value
    // Wire.beginTransmission(I2C_PH);
    // Wire.write(I2C_PH_REG_CALIB_START);
    // Wire.write(0x01);
    // Wire.endTransmission();

    // Write the calibration value to the register
    Wire.beginTransmission(I2C_PH);
    Wire.write(I2C_PH_REG_CALIB);
    Wire.write(((uint8_t*) &input)[3]);
    Wire.write(((uint8_t*) &input)[2]);
    Wire.write(((uint8_t*) &input)[1]);
    Wire.write(((uint8_t*) &input)[0]);
    Wire.endTransmission();

    // Wire.beginTransmission(I2C_PH);
    // Wire.write(I2C_PH_REG_POWER);
    // Wire.write(0x01);
    // Wire.endTransmission();
    // delay(I2C_PH_POWER_DELAY);

    // Write calibration request register
    Wire.beginTransmission(I2C_PH);
    Wire.write(I2C_PH_REG_CALIB_START);
    if (*finput == 0) {
      Wire.write(0x01);
    } else if (*finput <= 5.5) {
        Wire.write(0x02);
    } else if (*finput <= 8.5) {
        Wire.write(0x03);
    } else {
        Wire.write(0x04);
    }
    Wire.endTransmission();

    delay(I2C_PH_CALIB_DELAY);

    // Wire.beginTransmission(I2C_PH);
    // Wire.write(I2C_PH_REG_POWER);
    // Wire.write(0x00);
    // Wire.endTransmission();
}

// void doseAcid(int32_t *dtime, int32_t *out) {
//   digitalWrite(PDO_MOTOR1, HIGH);
//   digitalWrite(PDO_LED1, HIGH);
//   unsigned long delayt = *dtime;
//   delay(delayt);
//   digitalWrite(PDO_MOTOR1, LOW);
//   digitalWrite(PDO_LED1, LOW);
//   *out = 1;
// }

// void doseBase(int32_t *dtime, int32_t *out) {
//   digitalWrite(PDO_MOTOR2, HIGH);
//   digitalWrite(PDO_LED2, HIGH);
//   unsigned long delayt = *dtime;
//   delay(delayt);
//   digitalWrite(PDO_MOTOR2, LOW);
//   digitalWrite(PDO_LED2, LOW);
//   *out = 1;
// }

void setup() {
    pinMode(PDO_RELAY_LIGHT, OUTPUT);
    pinMode(PDO_RELAY_WATER, OUTPUT);
    pinMode(PAI_FLOW, INPUT);
    pinMode(PDO_MOTOR12_EN, OUTPUT);
    pinMode(PDO_MOTOR34_EN, OUTPUT);
    pinMode(PAI_TEMP_AIR, INPUT);
    pinMode(PAI_LIGHT, INPUT);
    pinMode(PAI_DEPTH_WATER, INPUT);
    pinMode(PDO_LED1, OUTPUT);
    pinMode(PDO_LED2, OUTPUT);

    pinMode(PDO_MOTOR1, OUTPUT);
    pinMode(PDO_MOTOR2, OUTPUT);
    pinMode(PDO_MOTOR3, OUTPUT);
    pinMode(PDO_MOTOR4, OUTPUT);

    digitalWrite(PDO_MOTOR12_EN, HIGH);
    digitalWrite(PDO_MOTOR1, LOW);
    digitalWrite(PDO_MOTOR2, LOW);

//    Serial.begin(115200);

    SerialPnP::Setup("Hydro");
    SerialPnP::NewInterface("http://contoso.com/hydro");

    SerialPnP::NewEvent("water_temperature",
                        "Water Temperature",
                        "x",
                        SerialPnPSchema::SchemaFloat,
                        "celsius");

    SerialPnP::NewEvent("ambient_temperature",
                        "Ambient Temperature",
                        "x",
                        SerialPnPSchema::SchemaFloat,
                        "celsius");

    SerialPnP::NewEvent("ambient_light",
                        "Ambient Light",
                        "x",
                        SerialPnPSchema::SchemaFloat,
                        "%");

    SerialPnP::NewEvent("flow",
                        "Flow Rate",
                        "x",
                        SerialPnPSchema::SchemaFloat,
                        "%");

    SerialPnP::NewEvent("water_depth",
                        "Water Depth",
                        "x",
                        SerialPnPSchema::SchemaFloat,
                        "%");

    SerialPnP::NewEvent("ec",
                        "Electrical Conductivity",
                        "x",
                        SerialPnPSchema::SchemaFloat,
                        "uS");

    SerialPnP::NewEvent("ph",
                        "pH",
                        "x",
                        SerialPnPSchema::SchemaFloat,
                        "pH");

    SerialPnP::NewEvent("dose_acid",
                        "Dose Acid",
                        "x",
                        SerialPnPSchema::SchemaInt,
                        "s");

    SerialPnP::NewEvent("dose_base",
                        "Dose Base",
                        "x",
                        SerialPnPSchema::SchemaInt,
                        "s");

    SerialPnP::NewProperty("sample_rate",
                           "Sample Rate",
                           "x",
                           "ms", // unit
                           SerialPnPSchema::SchemaInt,
                           false, // required
                           true, // writeable
                           (SerialPnPCb*) &CbSampleRate); // callback function on update

   SerialPnP::NewCommand("calibrate_ph",
                         "Calibrate pH",
                         "x",
                         SerialPnPSchema::SchemaFloat,
                         SerialPnPSchema::SchemaInt,
                         (SerialPnPCb*) &CbCalibratePh);

   // SerialPnP::NewCommand("dose_acid",
   //                      "Dose Acid",
   //                      "x",
   //                      SerialPnPSchema::SchemaInt,
   //                      SerialPnPSchema::SchemaInt,
   //                      (SerialPnPCb*) &doseAcid);

   // SerialPnP::NewCommand("dose_base",
   //                      "Dose Base",
   //                      "x",
   //                      SerialPnPSchema::SchemaInt,
   //                      SerialPnPSchema::SchemaInt,
   //                      (SerialPnPCb*) &doseBase);

    // Initialize I2C connection for our pH and EC sensors
    Wire.begin();

    // start pH, ec sampling
    Wire.beginTransmission(I2C_PH);
    Wire.write(I2C_PH_REG_POWER);
    Wire.write(0x01);
    Wire.endTransmission();

    Wire.beginTransmission(I2C_EC);
    Wire.write(I2C_EC_REG_POWER);
    Wire.write(0x01);
    Wire.endTransmission();

    // Find the onewire temp sensor
    #ifdef WATER_TEMP_SENSOR
    if (g_OneWireWaterTemp.search(g_tempSensorAddress)) {
        if (OneWire::crc8(g_tempSensorAddress, 7) != g_tempSensorAddress[7]) {
            g_tempSensorAddress[0] = 0; // 0 byte indicates invalid sensor
        }
    }
    #endif
}

float getWaterEc() {
    uint32_t input = 0;
    float ret = 0;
    uint8_t index = 3;

    Wire.beginTransmission(I2C_EC);
    Wire.write(I2C_EC_REG_STATUS);
    Wire.write(0x00);
    Wire.endTransmission();

    //Wire.beginTransmission(I2C_EC);
    //Wire.write(I2C_EC_REG_POWER);
    //Wire.write(0x01);
    //Wire.endTransmission();
    //delay(500);

    while (1) {
        Wire.beginTransmission(I2C_EC);
        Wire.write(I2C_EC_REG_STATUS);
        Wire.endTransmission();

        Wire.requestFrom(I2C_EC, 1);

        if (Wire.read() > 0) break;

        delay(I2C_EC_SAMPLE_DELAY);
    }

    Wire.beginTransmission(I2C_EC);
    Wire.write(I2C_EC_REG_DATA);
    Wire.endTransmission();

    Wire.requestFrom(I2C_EC, 4);
    while (Wire.available()) {
        ((uint8_t*) &input)[index--] = Wire.read();
    }

    //Wire.begin();
    //Wire.beginTransmission(I2C_EC);
    //Wire.write(I2C_EC_REG_POWER);
    //Wire.write(0x00);
    //Wire.endTransmission();

    ret = (float) input;
    ret /= 100;
    return ret;
}

float getWaterPh(float waterCelsius) {
    uint32_t input = 0;
    float ret = 0;
    uint8_t index = 3;
    //delay(I2C_PH_POWER_DELAY);

    // Write temperature
    if (waterCelsius != 0.0) {
      input = waterCelsius*100;
      Wire.beginTransmission(I2C_PH);
      Wire.write(I2C_PH_TEMP);
      Wire.write(((uint8_t*) &input)[3]);
      Wire.write(((uint8_t*) &input)[2]);
      Wire.write(((uint8_t*) &input)[1]);
      Wire.write(((uint8_t*) &input)[0]);
      Wire.endTransmission();
    }

    Wire.beginTransmission(I2C_PH);
    Wire.write(I2C_PH_REG_STATUS);
    Wire.write(0x00);
    Wire.endTransmission();

    //Wire.beginTransmission(I2C_PH);
    //Wire.write(I2C_PH_REG_POWER);
    //Wire.write(0x01);
    //Wire.endTransmission();

    input = 0;

    while (1) {
        Wire.beginTransmission(I2C_PH);
        Wire.write(I2C_PH_REG_STATUS);
        Wire.endTransmission();

        Wire.requestFrom(I2C_PH, 1);

        if (Wire.read() > 0) break;

        delay(I2C_PH_SAMPLE_DELAY);
    }

    Wire.beginTransmission(I2C_PH);
    Wire.write(I2C_PH_REG_DATA);
    Wire.endTransmission();

    Wire.requestFrom(I2C_PH, 4);
    while (Wire.available()) {
        ((uint8_t*) &input)[index--] = Wire.read();
    }

    //Wire.beginTransmission(I2C_PH);
    //Wire.write(I2C_PH_REG_POWER);
    //Wire.write(0x00);
    //Wire.endTransmission();

    ret = (float) input;
    ret /= 1000;
    return ret;
}

#ifdef WATER_TEMP_SENSOR
float getWaterTemperature() {
    byte i;
    byte data[12];
    float celsius, fahrenheit;

    if (g_tempSensorAddress[0] == 0) {
        return 0.0;
    }

    g_OneWireWaterTemp.reset();
    g_OneWireWaterTemp.select(g_tempSensorAddress);
    g_OneWireWaterTemp.write(0x44, 1);

    delay(750);

    g_OneWireWaterTemp.reset();
    g_OneWireWaterTemp.select(g_tempSensorAddress);
    g_OneWireWaterTemp.write(0xBE);         // Read Scratchpad

    for (i = 0; i < 9; i++) {           // we need 9 bytes
        data[i] = g_OneWireWaterTemp.read();
    }

    // Convert the data to actual temperature
    // because the result is a 16 bit signed integer, it should
    // be stored to an "int16_t" type, which is always 16 bits
    // even when compiled on a 32 bit processor.
    int16_t raw = (data[1] << 8) | data[0];

    byte cfg = (data[4] & 0x60);
    // at lower res, the low bits are undefined, so let's zero them
    if (cfg == 0x00) raw = raw & ~7;  // 9 bit resolution, 93.75 ms
    else if (cfg == 0x20) raw = raw & ~3; // 10 bit res, 187.5 ms
    else if (cfg == 0x40) raw = raw & ~1; // 11 bit res, 375 ms
    //// default is 12 bit resolution, 750 ms conversion time

    celsius = (float)raw / 16.0;
    //fahrenheit = celsius * 1.8 + 32.0;

    return celsius;
}
#endif

float getAmbientLight() {
    float val = 1024 - analogRead(PAI_LIGHT);
    if (val != 0) {
        val /= 1024;
    }

    return val * 100;
}

float getFlow() {
    float val = 1024 - analogRead(PAI_FLOW);
    if (val != 0) {
        val /= 1024;
    }

    return val * 100;
}

float getWaterDepth() {
    float val = analogRead(PAI_DEPTH_WATER);
    if (val != 0) {
        val /= 1024;
    }

    return val * 100;
}

float getAmbientTemperature() {
    float val = 1024 - analogRead(PAI_TEMP_AIR);
    if (val != 0) {
        val /= 1024;
    }

    return val;
}

void loop() {
  g_lastSample = millis();
  float waterTemp = 0.0;
  while(1) {
      SerialPnP::Process();

      uint32_t cticks = millis();

      // Determine if it's time to end the dosing logic
      #ifdef DOSE_PH
    if (g_millisFinishDose && (g_millisFinishDose < cticks)) {
        g_millisFinishDose = 0;
        g_millisLastDose = cticks;

        digitalWrite(PDO_MOTOR1, LOW);
        digitalWrite(PDO_LED1, LOW);

        digitalWrite(PDO_MOTOR2, LOW);
        digitalWrite(PDO_LED2, LOW);

        delay(PH_ADJUST_DOSE_PUMPWARMUP);
        digitalWrite(PDO_RELAY_WATER, LOW);
    }
    #endif

      if (cticks - g_lastSample < g_sampleRate) {
        continue;
      }

      g_lastSample = cticks;

      #ifdef WATER_TEMP_SENSOR
      waterTemp = getWaterTemperature();
      SerialPnP::SendEvent("water_temperature", waterTemp);
      #endif

      //SerialPnP::SendEvent("ambient_temperature", getAmbientTemperature());
      //SerialPnP::SendEvent("ambient_light", getAmbientLight());
      //SerialPnP::SendEvent("flow", getFlow());
      //SerialPnP::SendEvent("water_depth", getWaterDepth());
      SerialPnP::SendEvent("ec", getWaterEc());

      float ph = getWaterPh(waterTemp);
      SerialPnP::SendEvent("ph", ph);

  // refresh timer as sampling the sensors may take a couple of seconds
     cticks = millis();

    // It only makes sense to run dosing logic after we take a fresh sample
    // Start the dose if appropriate
    #ifdef DOSE_PH
    if (!g_millisFinishDose &&
        //0 &&
        (ph >= PH_ADJUST_LOGIC_ACTIVE_MIN) &&
        (ph <= PH_ADJUST_LOGIC_ACTIVE_MAX) &&
        (/* !g_millisLastDose ||  */ (cticks - g_millisLastDose > PH_ADJUST_DOSE_INTERVAL))) {

        // Check if pH is below acceptable bounds
        if (ph < PH_ADJUST_THRESHOLD_MIN) {
            SerialPnP::SendEvent("dose_base", (int32_t) (PH_ADJUST_DOSE_TIME/1000));

            // enable pump
            digitalWrite(PDO_RELAY_WATER, HIGH);
            delay(PH_ADJUST_DOSE_PUMPWARMUP);

            // dose base
            g_millisFinishDose = millis() + PH_ADJUST_DOSE_TIME;

            digitalWrite(PDO_MOTOR2, HIGH);
            digitalWrite(PDO_LED2, HIGH);
        }

        else if (ph > PH_ADJUST_THRESHOLD_MAX) {
            SerialPnP::SendEvent("dose_acid", (int32_t) (PH_ADJUST_DOSE_TIME/1000));

            // enable pump
            digitalWrite(PDO_RELAY_WATER, HIGH);
            delay(PH_ADJUST_DOSE_PUMPWARMUP);

            // dose acid
            g_millisFinishDose = millis() + PH_ADJUST_DOSE_TIME;

            digitalWrite(PDO_MOTOR1, HIGH);
            digitalWrite(PDO_LED1, HIGH);
        }
    }
    #endif
  }
}
