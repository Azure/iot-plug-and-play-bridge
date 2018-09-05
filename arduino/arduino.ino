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

// the setup function runs once when you press reset or power the board
void setup() {        
  
  // initialize digital pins
  pinMode(LED_BUILTIN, OUTPUT);
  pinMode(MOISTURE_SENSOR_VCC_1, OUTPUT);
  pinMode(MOISTURE_SENSOR_VCC_2, OUTPUT);
  pinMode(LIGHT_SWITCH_1, OUTPUT);
  pinMode(PUMP_SWITCH_1, OUTPUT);

   SerialPnP::Setup("xGrow");
   SerialPnP::NewInlineInterface("http://contoso.com/exampleint");
  
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

  SerialPnP::NewProperty("sample_rate",
                          "Sample Rate",
                          "Sample rate for sensors",
                          "ms", // unit
                          false, // required
                          true, // writeable
                          (SerialPnPCb*) CbSampleRate); // callback function on update

  SerialPnP::NewCommand("set_pump",
                         "Set Pump",
                         "Set state of water pump",
                         SerialPnPSchema::SchemaBool, // "schema" of input parameter
                         SerialPnPSchema::SchemaBool, // "schema" of output parameter
                         (SerialPnPCb*) setPumpSwitch); // callback function on execution

    SerialPnP::NewCommand("set_light",
                         "Set light",
                         "Set state of grow light",
                         SerialPnPSchema::SchemaBool,
                         SerialPnPSchema::SchemaBool,
                         (SerialPnPCb*) setLightSwitch);
 
 }

float getMoistureSensor(int number)
{
  float val = 0;
  if (number == 1)
  {
    digitalWrite(MOISTURE_SENSOR_VCC_1, HIGH);
    delay(20);
    val = 1024 - analogRead(MOISTURE_SENSOR_1);
    digitalWrite(MOISTURE_SENSOR_VCC_1, LOW);
  }
  else if (number == 2)
  {
    digitalWrite(MOISTURE_SENSOR_VCC_2, HIGH);
    delay(20);
    val = 1024 - analogRead(MOISTURE_SENSOR_2);
    digitalWrite(MOISTURE_SENSOR_VCC_2, LOW);  
  }
  
  val /= 1024;
}

float getLightSensor()
{
  float val = 1024 - analogRead(LIGHT_SENSOR_1);
  val /= 1024;

  return val;
}

void setLightSwitch(uint8_t *in, uint8_t *out)
{
  if (*in == 1)
  {
    digitalWrite(LIGHT_SWITCH_1, HIGH);
  }
  else
  {
    digitalWrite(LIGHT_SWITCH_1, LOW);  
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

      rep = getMoistureSensor(1);
      SerialPnP::SendEvent("moisture", rep);
  }
}
