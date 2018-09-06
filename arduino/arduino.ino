#include "SerialPnP.h"

int g_propcount = 0;

void CbSampleRate (uint32_t* Input, uint32_t* Output) {
  if (Input) {
    g_propcount = (uint8_t) *Input;
  }
  
  if (Output) {
    *Output = ++g_propcount;
  }
}

void CbPump (uint8_t *Input, uint8_t *Output) {
  *Output = *Input;

  if (*Input == 0) {
    digitalWrite(LED_BUILTIN, LOW);
  } else {
    digitalWrite(LED_BUILTIN, HIGH);
  }
}


void setup() {
  // put your setup code here, to run once:
  pinMode(LED_BUILTIN, OUTPUT);
  
  SerialPnP::Setup("xGrow");

  SerialPnP::NewInlineInterface("http://contoso.com/exampleint");

    SerialPnP::NewProperty("sample_rate",
                          "Sample Rate",
                          "Sample rate for sensors",
                          "ms", // unit
                          true, // required
                          true, // writeable
                          (SerialPnPCb*) CbSampleRate); // callback function on update
  
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

  SerialPnP::NewCommand("set_pump",
                         "Set Pump",
                         "Set state of water pump",
                         SerialPnPSchema::SchemaBool, // "schema" of input parameter
                         SerialPnPSchema::SchemaBool, // "schema" of output parameter
                         (SerialPnPCb*) CbPump); // callback function on execution

    SerialPnP::NewCommand("set_light",
                         "Set light",
                         "Set state of grow light",
                         SerialPnPSchema::SchemaBool,
                         SerialPnPSchema::SchemaBool,
                         nullptr);
}

void loop() {
  // put your main code here, to run repeatedly:

  while(1) {
    SerialPnP::Process();

  delay(1000);
  //SerialPnP::SendEvent("ambient_light", 25.25f);
  SerialPnP::SendEvent("moisture", 50.05f);
  }
}
