//
// Serial PnP Arduino Platform Functions
//
// Copyright (c) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE file in the project root for full license information.
//
#include <Arduino.h>
#include <Wire.h>
#include "SerialPnP.h"

// This function will be called during SerialPnP initialization, and should
// configure the serial port for SerialPnP operation, enabling it with a baud
// rate of 115200 and other settings compliant with the Serial PnP specification.
void
SerialPnP_PlatformSerialInit()
{
    Serial.begin(115200);
    Serial.flush();
}

// This function should return the number of buffered characters available from
// the serial port used for SerialPnP operation.
unsigned int
SerialPnP_PlatformSerialAvailable()
{
    return Serial.available();
}

// This function should return the next buffered character read from the
// serial port used for SerialPnP operation. If no characters are available,
// it shall return -1.
int
SerialPnP_PlatformSerialRead()
{
    return Serial.read();
}

// This function should write a single character to the serial port to be used
// for SerialPnP operation.
void
SerialPnP_PlatformSerialWrite(
    char            Character
)
{
    Serial.write(Character);
}

// This function should reset the state of the device.
void
SerialPnP_PlatformReset()
{
    // This call must be made once to complete Serial PnP Setup, after all interaces have been defined.
    // It will notify the host that device initialization is complete.
    SerialPnP_Ready();
}
