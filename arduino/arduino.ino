/*
*/

#include <Arduino.h>

#define MOISTURE_SENSOR_1 A0
#define MOISTURE_SENSOR_2 A2
#define LIGHT_SENSOR_1 A1

#define MOISTURE_SENSOR_VCC_1 2
#define MOISTURE_SENSOR_VCC_2 5

#define LIGHT_SWITCH_1 3
#define PUMP_SWITCH_1 4

#include "SerialPnP.h"

uint16_t g_sampleRate = 1000;

long g_lastSample = 0;

void CbSampleRate(uint16_t *input, uint16_t *output) {
  if (output) *output = g_sampleRate;

  if (input) g_sampleRate = *input;
}

void CbTest(uint8_t *input, uint8_t *output) {
  if (input) {
    output[0] = 2*input[0];
    output[1] = 2*input[1];
    output[2] = 2*input[2];
    output[3] = 2*input[3];
  } else {
    output[0] = 0xFE;
    output[1] = 0xEF;
    output[2] = 0xDE;
    output[3] = 0xED;
  }
}

// the setup function runs once when you press reset or power the board
void setup() {        
  
  // initialize digital pins
  pinMode(LED_BUILTIN, OUTPUT);
  pinMode(MOISTURE_SENSOR_VCC_1, OUTPUT);
  pinMode(MOISTURE_SENSOR_VCC_2, OUTPUT);
  pinMode(LIGHT_SWITCH_1, OUTPUT);
  pinMode(PUMP_SWITCH_1, OUTPUT);

   SerialPnP::Setup("xGrow");
   SerialPnP::NewInterface("http://contoso.com/exampleint");
  
  SerialPnP::NewEvent("ambient_light",
                     "Ambient Light",
                     "Ambient light on the plant",
                     SerialPnPSchema::SchemaFloat, // "schema" of data
                     "%"); // units (just a percentage as a float)

  SerialPnP::NewEvent("moisture",
                       "Moisture",
                       "Moisture in the soil",
                       SerialPnPSchema::SchemaFloat,
                       "%");

//  SerialPnP::NewProperty("sample_rate",
//                          "Sample Rate",
//                          "Sample rate for sensors",
//                          "ms", // unit
//                          SerialPnPSchema::SchemaInt,
//                          false, // required
//                          true, // writeable
//                          (SerialPnPCb*) CbSampleRate); // callback function on update

SerialPnP::NewProperty("test",
                          "Sample Rate",
                          "Sample rate for sensors",
                          "ms", // unit
                          SerialPnPSchema::SchemaInt,
                          false, // required
                          true, // writeable
                          (SerialPnPCb*) CbTest); // callback function on update

  // SerialPnP::NewCommand("set_pump",
  //                        "Set Pump",
  //                        "Set state of water pump",
  //                        SerialPnPSchema::SchemaBoolean, // "schema" of input parameter
  //                        SerialPnPSchema::SchemaBoolean, // "schema" of output parameter
  //                        (SerialPnPCb*) setPumpSwitch); // callback function on execution

    SerialPnP::NewCommand("set_light",
                         "Set light",
                         "Set state of grow light",
                         SerialPnPSchema::SchemaBoolean,
                         SerialPnPSchema::SchemaBoolean,
                         (SerialPnPCb*) setLightSwitch);
 
 }

float getMoistureSensor()
{
  float val = 0;

  val = 1024 - analogRead(MOISTURE_SENSOR_1);

  if (val != 0) {
    val /= 1024;
  }
  return val;
}

float getLightSensor()
{
  float val = 1024 - analogRead(LIGHT_SENSOR_1);
  if (val != 0) {
    val /= 1024;
  }

  return val;
}

void setLightSwitch(uint8_t *in, uint8_t *out)
{
  // if (*in == 1)
  // {
  //   digitalWrite(LIGHT_SWITCH_1, HIGH);
  // }
  // else
  // {
  //   digitalWrite(LIGHT_SWITCH_1, LOW);  
  // }

  if (*in == 0) {
    digitalWrite(LED_BUILTIN, LOW);
  } else {
    digitalWrite(LED_BUILTIN, HIGH);
  }

  *out = *in;
}

void setPumpSwitch(uint8_t *in, uint8_t *out)
{
  if (*in)
  {
    digitalWrite(PUMP_SWITCH_1, HIGH);  
  }
  else
  {
    digitalWrite(PUMP_SWITCH_1, LOW);
  }

  *out = *in;
}

// the loop function runs over and over again forever
void loop() {
  g_lastSample = millis();
  while(1) {
      SerialPnP::Process();

      long cticks = millis();
      if (cticks - g_lastSample < g_sampleRate) {
        continue;
      }

      g_lastSample = cticks;

      float rep;

      rep = getLightSensor();
      SerialPnP::SendEvent("ambient_light", rep);

      rep = getMoistureSensor();
      //SerialPnP::SendEvent("moisture", rep);
  }
}
