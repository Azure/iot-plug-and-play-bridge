// Copyright (c) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE file in the project root for full license information.

#pragma once

#ifdef __cplusplus
extern "C"
{
#endif

// Core device context
typedef struct _CORE_DEVICE_TAG {
    // Azure IOT PnP Interface handle
    DIGITALTWIN_INTERFACE_CLIENT_HANDLE DigitalTwinInterface;

    // Windows PnP event notification registration handle
    HCMNOTIFICATION NotifyHandle;
    
    // Device symbolic link
    char* SymbolicLink;

    // Device active
    bool DeviceActive;
} CORE_DEVICE_TAG, *PCORE_DEVICE_TAG;

typedef struct _CORE_DEVICE_ADAPTER_CONTEXT {
    SINGLYLINKEDLIST_HANDLE SupportedInterfaces;
    SINGLYLINKEDLIST_HANDLE DeviceWatchers;
} CORE_DEVICE_ADAPTER_CONTEXT, *PCORE_DEVICE_ADAPTER_CONTEXT;

int
CoreDevice_SendConnectionEventAsync(
    _In_ DIGITALTWIN_INTERFACE_CLIENT_HANDLE DigitalTwinInterface,
    _In_ char* EventName,
    _In_ char* EventData
    );

#define CONN_EVENT_SIZE 512
#define CONN_FORMAT "{\"%s\":%s}"

#ifdef __cplusplus
}
#endif