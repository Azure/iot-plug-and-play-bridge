// Copyright (c) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE file in the project root for full license information.

/*
  ___           ___     _    _
 | _ \_ _  _ __| _ )_ _(_)__| |__ _ ___
 |  _/ ' \| '_ \ _ \ '_| / _` / _` / -_)
 |_| |_||_| .__/___/_| |_\__,_\__, \___|
		  |_|                 |___/
*/

// TODO: REMOVE THE COMMENTS
// NOTES TO SELF:

//
// Config update requires bridge tear down and restart
//

//
// Discovered Device -> Bind -> CreatePnpInterfaces -> Create Pnp Interface client ->
// Create Pnp Adapter Interface -> Associate it with the adapter and store the device config pointer -> Get All interface for all adapters -> publish
//

/*

0. TEST FOR MEMORY LEAKS USING APPVERIFIER

SerialPnp
 1. Closing serial handle
 2. Test publishing 2 interfaces (multiple)
 3. Test overlapped IO

PnpBridge
 4. DPS connection & test
 5. FT to create a tear down in a loop *** MOCK the PNP CLIENT SDK
 6. Release PnpInterfaces..
 7. Persistent device should get callback when device is discovered

CoreDevice
 7. Enumerate devices
 8. Add properties based on original device schema

CMAKE
 9. Clean up warnings to get a clean build

*/
// ***************

#pragma once

#ifdef __cplusplus
extern "C"
{
#endif

#include <pnp_interface_client.h>
#include <core/PnpBridgeMemory.h>

typedef enum _PNPBRIDGE_INTERFACE_CHANGE_TYPE {
    PNPBRIDGE_INTERFACE_CHANGE_INVALID,
    PNPBRIDGE_INTERFACE_CHANGE_ARRIVAL,
    PNPBRIDGE_INTERFACE_CHANGE_PERSIST,
    PNPBRIDGE_INTERFACE_CHANGE_REMOVAL
} PNPBRIDGE_INTERFACE_CHANGE_TYPE;

typedef struct _PNPBRIDGE_DEVICE_CHANGE_PAYLOAD {
    const char* Message;
    int MessageLength;
    PNPBRIDGE_INTERFACE_CHANGE_TYPE ChangeType;
    void* Context;
} PNPBRIDGE_DEVICE_CHANGE_PAYLOAD, *PPNPBRIDGE_DEVICE_CHANGE_PAYLOAD;

#include <DiscoveryAdapterInterface.h>
#include <PnpAdapterInterface.h>

#ifdef __cplusplus
}
#endif