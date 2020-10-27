// Copyright (c) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE file in the project root for full license information.

#pragma once

#ifdef __cplusplus
extern "C"
{
#endif

// Core device context
typedef struct _CORE_DEVICE_TAG {
    // Azure IoT PnP Interface handle
    IOTHUB_DEVICE_CLIENT_HANDLE DeviceClient;

    // Windows PnP event notification registration handle
    HCMNOTIFICATION NotifyHandle;

    // Device symbolic link
    char* SymbolicLink;

    // Device active state
    bool DeviceActive;

    // Component Name
    char* ComponentName;

    // Telemetry Started
    bool TelemetryStarted;
} CORE_DEVICE_TAG, *PCORE_DEVICE_TAG;

typedef struct _CORE_DEVICE_ADAPTER_CONTEXT {
    SINGLYLINKEDLIST_HANDLE SupportedInterfaces;
    SINGLYLINKEDLIST_HANDLE DeviceWatchers;
} CORE_DEVICE_ADAPTER_CONTEXT, *PCORE_DEVICE_ADAPTER_CONTEXT;

IOTHUB_CLIENT_RESULT
CoreDevice_SendConnectionEventAsync(
    _In_ PCORE_DEVICE_TAG DeviceContext,
    _In_ char* EventName,
    _In_ char* EventData
    );

#define CONN_EVENT_SIZE 512
#define CONN_FORMAT "{\"%s\":%s}"

#ifdef __cplusplus
}
#endif