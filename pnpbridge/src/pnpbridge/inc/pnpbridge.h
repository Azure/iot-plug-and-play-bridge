// Copyright (c) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE file in the project root for full license information.

/*
  ___           ___     _    _
 | _ \_ _  _ __| _ )_ _(_)__| |__ _ ___
 |  _/ ' \| '_ \ _ \ '_| / _` / _` / -_)
 |_| |_||_| .__/___/_| |_\__,_\__, \___|
          |_|                 |___/
*/

#pragma once

#ifndef PNPBRIDGE_H
#define PNPBRIDGE_H

#if !defined(_MSC_VER)
#include <nosal.h>
#endif

#include "azure_macro_utils/macro_utils.h"
#include "umock_c/umock_c_prod.h"

// PnP helper utilities.
#include "pnp_device_client_ll.h"
#include "pnp_dps_ll.h"
#include "pnp_protocol.h"

#include "pnpadapter_api.h"
#include "pnpbridge_common.h"

#ifdef __cplusplus
extern "C"
{
#endif

typedef void* MX_IOT_HANDLE;

MOCKABLE_FUNCTION(, int, PnpBridge_Main, const char*, ConfigurationFilePath);

MOCKABLE_FUNCTION(, void, PnpBridge_Stop);

MOCKABLE_FUNCTION(,
int,
PnpBridge_UploadToBlobAsync,
    const char*, pszDestination,
    const unsigned char*, pbData,
    size_t, cbData,
    IOTHUB_CLIENT_FILE_UPLOAD_CALLBACK, iotHubClientFileUploadCallback,
    void*, context
    );

#ifdef __cplusplus
}
#endif
#endif
