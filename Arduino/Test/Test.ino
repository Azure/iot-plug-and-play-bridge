#include <Arduino.h>

#include "SerialPnP.h"

int32_t g_reportRate = 1000;
int32_t g_counter = 0;

long g_lastReport = 0;

void CbSampleRate(int32_t *input, int32_t *output) {
  if (output) *output = g_reportRate;

  if (input) g_reportRate = *input;
}

void CbDouble(int32_t *input, int32_t *output) {
  *output = *input << 1;
}

// the setup function runs once when you press reset or power the board
void setup() {
    SerialPnP::Setup("Test");
    SerialPnP::NewInterface("http://contoso.com/spnptest");

    SerialPnP::NewEvent("counter",
                        "Counter",
                        "Continuous count speed",
                        SerialPnPSchema::SchemaInt, // "schema" of data
                        "tick"); // units (just a percentage as a float)

    SerialPnP::NewProperty("count_rate",
                           "Counter Rate",
                           "Time per tick of counter",
                           "ms", // unit
                           SerialPnPSchema::SchemaInt,
                           false, // required
                           true, // writeable
                           (SerialPnPCb*) CbSampleRate); // callback function on update
    SerialPnP::NewCommand("double",
                          "Multiply by 2",
                          "Doubles a value and returns the input",
                          SerialPnPSchema::SchemaInt,
                          SerialPnPSchema::SchemaInt,
                          (SerialPnPCb*) CbDouble);
}

// the loop function runs over and over again forever
void loop() {
  g_lastReport = millis();
  while(1) {
      SerialPnP::Process();

      long cticks = millis();
      if (cticks - g_lastReport < g_reportRate) {
        continue;
      }

      g_lastReport = cticks;
      ++g_counter;
      SerialPnP::SendEvent("counter", (uint8_t*) &g_counter, sizeof(g_counter));
  }
}
