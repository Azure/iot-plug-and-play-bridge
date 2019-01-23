//
// Serial PnP Device-Side Library
//
// Copyright (c) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE file in the project root for full license information.
//
#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

// This may be adjusted to support longer schemas in the future.
#define SERIALPNP_RXBUFFER_SIZE         64
#define SERIALPNP_MAX_CALLBACK_COUNT    8

//
// PLATFORM-SPECIFIC FUNCTIONS
// These functions must be implemented by the platform and made available
// to Serial PnP.
//

// This function will be called during SerialPnP initialization, and should
// configure the serial port for SerialPnP operation, enabling it with a baud
// rate of 115200 and other settings compliant with the Serial PnP specification.
void
SerialPnP_PlatformSerialInit();

// This function should return the number of buffered characters available from
// the serial port used for SerialPnP operation.
unsigned int
SerialPnP_PlatformSerialAvailable();

// This function should return the next buffered character read from the
// serial port used for SerialPnP operation. If no characters are available,
// it shall return -1.
int
SerialPnP_PlatformSerialRead();

// This function should write a single character to the serial port to be used
// for SerialPnP operation.
void
SerialPnP_PlatformSerialWrite(
    char            Character
);

// This function should reset the state of the device.
void
SerialPnP_PlatformReset();

//
// SERIAL PNP INTERFACE
//
typedef void (*SerialPnPCb)(void*, void*);

typedef enum _SerialPnPSchema {
    SerialPnPSchema_None = 0,
    SerialPnPSchema_Byte,
    SerialPnPSchema_Float,
    SerialPnPSchema_Double,
    SerialPnPSchema_Int,
    SerialPnPSchema_Long,
    SerialPnPSchema_Boolean,
    SerialPnPSchema_String,
} SerialPnPSchema;

// This function is called once to configure Serial PnP on the platform.
void
SerialPnP_Setup(
    const char*     DeviceName
);

// This call must be made once to complete Serial PnP Setup, after all interaces have been defined.
// It will notify the host that device initialization is complete.
void
SerialPnP_Ready();

// This function is called once per interface to be exposed by the device.
void
SerialPnP_NewInterface(
    const char*     InterfaceIdUri
);

void
SerialPnP_NewEvent(
    const char*     Name,
    const char*     DisplayName,
    const char*     Description,
    SerialPnPSchema Schema,
    const char*     Units
);

void
SerialPnP_NewProperty(
    const char*     Name,
    const char*     DisplayName,
    const char*     Description,
    const char*     Units,
    SerialPnPSchema Schema,
    bool            Required,
    bool            Writeable,
    SerialPnPCb*    Callback
);

void
SerialPnP_NewCommand(
    const char*     Name,
    const char*     DisplayName,
    const char*     Description,
    SerialPnPSchema InputSchema,
    SerialPnPSchema OutputSchema,
    SerialPnPCb*    Callback
);

// Must be called regularly and frequently to process any incoming commands or
// other protocol operations.
void
SerialPnP_Process();

void
SerialPnP_SendEventFloat(
    const char*     Name,
    float           Value
);

void
SerialPnP_SendEventInt(
    const char*     Name,
    int32_t         Value
);

#ifdef __cplusplus
}
#endif