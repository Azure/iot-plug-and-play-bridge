// Copyright (c) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE file in the project root for full license information.

#ifndef PNPBRIDGE_DISCOVERY_ADAPTER_INTERFACE_H
#define PNPBRIDGE_DISCOVERY_ADAPTER_INTERFACE_H

#ifdef __cplusplus
extern "C"
{
#endif

    /**
    * @brief    PNPBRIDGE_NOTIFY_DEVICE_CHANGE callback is called by the discovery adapters.
    *
    * @remarks  When a discovery adapter finds a device, it needs to call PNPBRIDGE_NOTIFY_DEVICE_CHANGE
    callback that is provided when as an argument to StartDiscovery callback. The adapter
    should populate the DeviceChangePayload with appropriate device details that will be
    consumed by any supporting pnp adapter to publish Azure Pnp Interface.

    * @param    DeviceChangePayload           Message containing device details.
    *
    * @returns  integer greater than zero on success and other values on failure.
    */
    typedef void(*PNPBRIDGE_NOTIFY_DEVICE_CHANGE)(PPNPBRIDGE_DEVICE_CHANGE_PAYLOAD DeviceChangePayload);

    /**
    * @brief    DISCOVERYAPAPTER_START_DISCOVER callback is called to notify a discovery adapter to start discovery.
    *

    * @param    DeviceChangeCallback           PnpBridge callback method to be called when an adapter discovers device.
    *
    * @param    deviceArgs                     Json object containing list of devices arguments provided in config.
    *
    * @param    adapterArgs                    Json object containing adapter arguments provided in config.
    *
    * @returns  integer greater than zero on success and other values on failure.
    */
    typedef int(*DISCOVERYAPAPTER_START_DISCOVER)(PNPBRIDGE_NOTIFY_DEVICE_CHANGE DeviceChangeCallback, const char* deviceArgs, const char* adapterArgs);

    /**
    * @brief    DISCOVERYAPAPTER_STOP_DISCOVERY callback is called to notify a discovery adapter to stop discovery.
    *

    * @returns  integer greater than zero on success and other values on failure.
    */
    typedef int(*DISCOVERYAPAPTER_STOP_DISCOVERY)();

    typedef struct _DISCOVERY_ADAPTER {
        const char* Identity;
        DISCOVERYAPAPTER_START_DISCOVER StartDiscovery;
        DISCOVERYAPAPTER_STOP_DISCOVERY StopDiscovery;
    } DISCOVERY_ADAPTER, *PDISCOVERY_ADAPTER;

    int PnpBridge_DeviceChangeCallback(PPNPBRIDGE_DEVICE_CHANGE_PAYLOAD DeviceChangePayload);

#ifdef __cplusplus
}
#endif

#endif