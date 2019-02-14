// Copyright (c) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE file in the project root for full license information.

#pragma once

#ifdef __cplusplus
extern "C"
{
#endif

#include <pnp_interface_client.h>

typedef enum _PNPBRIDGE_INTERFACE_CHANGE_TYPE {
    PNPBRIDGE_INTERFACE_CHANGE_INVALID,
    PNPBRIDGE_INTERFACE_CHANGE_ARRIVAL,
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

int
PnpBridge_UploadToBlobAsync(
    _In_z_ const char* pszDestination,
    _In_reads_bytes_(cbData) const unsigned char* pbData,
    _In_ size_t cbData, 
    _In_ IOTHUB_CLIENT_FILE_UPLOAD_CALLBACK iotHubClientFileUploadCallback, 
    _In_opt_ void* context
    );

#ifdef __cplusplus
}
#endif