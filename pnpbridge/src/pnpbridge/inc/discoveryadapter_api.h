// Copyright (c) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE file in the project root for full license information.

/**  @file discovery_adapter_interface.h
*    @brief  Discovery Adapter functions.
*
*    @details  DiscoveryAdapter is an extension point in PnpBridge that allows developers to author
new adapters that can watch for device arrival and notify PnPBridge with a device change message whose
format is specified by the PnPBridge.
TODOC: PROVIDE DETAILS OF PNPMESSAGE
*/

#ifndef PNPBRIDGE_DISCOVERY_ADAPTER_INTERFACE_H
#define PNPBRIDGE_DISCOVERY_ADAPTER_INTERFACE_H

#include "azure_macro_utils/macro_utils.h"
#include "umock_c/umock_c_prod.h"

#ifdef __cplusplus
extern "C"
{
#endif

typedef struct _DEVICE_ADAPTER_PARMAETERS {
    // Number of devices containing adapter parameters for a given adapter
    int Count;

    // Array of serialized json device adapter 
    // parameters of size equal to Count
    char* AdapterParameters[1];
} DEVICE_ADAPTER_PARMAETERS, *PDEVICE_ADAPTER_PARMAETERS;

/**
* @brief    PNPBRIDGE_NOTIFY_DEVICE_CHANGE callback is called by the discovery adapters.
*
* @remarks  When a discovery adapter finds a device, it needs to call PNPBRIDGE_NOTIFY_DEVICE_CHANGE
callback that is provided as an argument to *StartDiscovery* callback. The adapter
should populate the DeviceChangePayload with appropriate device details that will be
consumed by any supporting pnp adapter in order to publish Azure Pnp Interface.

* @param    DeviceChangePayload           PNPMESSAGE containing device details.
*
* @returns  integer greater than zero on success and other values on failure.
*/
//PPNPBRIDGE_DEVICE_CHANGE_PAYLOAD
typedef int
(*PNPBRIDGE_NOTIFY_CHANGE)(
    _In_ PNPMESSAGE PnpMessage
    );

/**
* @brief    DISCOVERYAPAPTER_START_DISCOVER callback is called to notify a discovery adapter to start discovery.
*

* @param    DeviceChangeCallback           PnpBridge callback method to be called when an adapter discovers device.
*
* @param    deviceArgs                     Json string PNPMEMORY containing list of devices arguments provided in config.
*
* @param    adapterArgs                    Json string PNPMEMORY containing adapter arguments provided in config.
*
* @returns  integer greater than zero on success and other values on failure.
*/
typedef int
(*DISCOVERYADAPTER_START_DISCOVER)(
    _In_ PNPMEMORY DeviceArgs, 
    _In_ PNPMEMORY AdapterArgs
    );

/**
* @brief    DISCOVERYAPAPTER_STOP_DISCOVERY callback is called to notify a discovery adapter to stop discovery.
*

* @returns  integer greater than zero on success and other values on failure.
*/
typedef int
(*DISCOVERYADAPTER_STOP_DISCOVERY)();

/**
* @brief    DiscoveryAdapter_ReportDevice notifies the pnpbridge of a new device discovery/removal
*

* @returns  integer greater than zero on success and other values on failure.
*/

MOCKABLE_FUNCTION(,
int,
DiscoveryAdapter_ReportDevice,
    _In_ PNPMESSAGE, Message
    );

typedef struct _DISCOVERY_ADAPTER {
    const char* Identity;

    DISCOVERYADAPTER_START_DISCOVER StartDiscovery;

    DISCOVERYADAPTER_STOP_DISCOVERY StopDiscovery;
} DISCOVERY_ADAPTER, *PDISCOVERY_ADAPTER;

#ifdef __cplusplus
}
#endif

#endif