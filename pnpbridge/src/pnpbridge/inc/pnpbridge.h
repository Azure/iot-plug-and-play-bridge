// Copyright (c) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE file in the project root for full license information.

/*
  ___           ___     _    _
 | _ \_ _  _ __| _ )_ _(_)__| |__ _ ___
 |  _/ ' \| '_ \ _ \ '_| / _` / _` / -_)
 |_| |_||_| .__/___/_| |_\__,_\__, \___|
          |_|                 |___/
*/

#ifndef PNPBRIDGE_H
#define PNPBRIDGE_H

#include <digitaltwin_interface_client.h>

#ifdef __cplusplus
extern "C"
{
#endif

#if !defined(_MSC_VER)
#include <nosal.h>
#endif

#include <pnpbridge_memory.h>

typedef void* MX_IOT_HANDLE;

DIGITALTWIN_DEVICE_CLIENT_HANDLE IotHandle_GetPnpDeviceClient(MX_IOT_HANDLE IotHandle);

#include <pnpmessage_api.h>
#include <discoveryadapter_api.h>
#include <pnpadapter_api.h>

int
PnpBridge_Main();

void
PnpBridge_Stop();

int
PnpBridge_UploadToBlobAsync(
    const char* pszDestination,
    const unsigned char* pbData,
    size_t cbData,
    IOTHUB_CLIENT_FILE_UPLOAD_CALLBACK iotHubClientFileUploadCallback,
    void* context
);

#ifdef __cplusplus
}
#endif
#endif
