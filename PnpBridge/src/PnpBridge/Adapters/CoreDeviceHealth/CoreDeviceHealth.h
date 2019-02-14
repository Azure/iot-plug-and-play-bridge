// Copyright (c) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE file in the project root for full license information.

#pragma once

#ifdef __cplusplus
extern "C"
{
#endif

typedef struct _CORE_DEVICE_TAG {
    PNP_INTERFACE_CLIENT_HANDLE pnpinterfaceHandle;
    HCMNOTIFICATION hNotifyCtx;
    char* symbolicLink;
    volatile bool state;
} CORE_DEVICE_TAG, *PCORE_DEVICE_TAG;

#ifdef __cplusplus
}
#endif