// Copyright (c) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE file in the project root for full license information.

#ifdef __cplusplus
extern "C"
{
#endif

// Device aggregator context
typedef struct _PNP_BRIDGE {
    // connection to iot device
    IOTHUB_DEVICE_HANDLE deviceHandle;

    // Handle representing PnpDeviceClient
    PNP_DEVICE_CLIENT_HANDLE pnpDeviceClientHandle;

    // Manages loading all discovery plugins and their lifetime
    PDISCOVERY_MANAGER discoveryMgr;

    // Manages loading all pnp adapter plugins and their lifetime
    PPNP_ADAPTER_MANAGER adapterMgr;

    LOCK_HANDLE dispatchLock;

    // PnpBridge config document
    JSON_Value* configuration;

    bool shutdown;

} PNP_BRIDGE, *PPNP_BRIDGE;

void PnpBridge_Release(PPNP_BRIDGE pnpBridge);

#ifdef __cplusplus
}
#endif